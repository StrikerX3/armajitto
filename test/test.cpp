#include <armajitto/armajitto.hpp>

#include <SDL2/SDL.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>

class MinimalNDSSystem : public armajitto::ISystem {
public:
    MinimalNDSSystem() {
        mainRAM.fill(0);
        sharedWRAM.fill(0);
        vram.fill(0);

        using MemArea = armajitto::MemoryMap::Areas;

        m_memMap.Map(MemArea::All, 0, 0x2000000, 0x1000000, mainRAM.data(), mainRAM.size());
        m_memMap.Map(MemArea::All, 0, 0x3000000, 0x1000000, sharedWRAM.data(), sharedWRAM.size());
        m_memMap.Map(MemArea::All, 0, 0x6800000, 0xA4000, vram.data(), vram.size());
    }

    uint8_t MemReadByte(uint32_t address) final {
        return Read<uint8_t>(address);
    }
    uint16_t MemReadHalf(uint32_t address) final {
        return Read<uint16_t>(address);
    }
    uint32_t MemReadWord(uint32_t address) final {
        return Read<uint32_t>(address);
    }

    void MemWriteByte(uint32_t address, uint8_t value) final {
        Write(address, value);
    }
    void MemWriteHalf(uint32_t address, uint16_t value) final {
        Write(address, value);
    }
    void MemWriteWord(uint32_t address, uint32_t value) final {
        Write(address, value);
    }

    void CopyToRAM(uint32_t baseAddress, const uint8_t *data, uint32_t size) {
        if ((baseAddress >> 24) == 0x02) {
            baseAddress &= 0x3FFFFF;
            size = std::min(size, 0x400000 - baseAddress);
            std::copy_n(data, size, mainRAM.begin() + baseAddress);
        }
    }

    std::array<uint8_t, 0x400000> mainRAM;
    std::array<uint8_t, 0x8000> sharedWRAM;
    std::array<uint8_t, 0xA4000> vram;

    template <typename T>
    T Read(uint32_t address) {
        auto page = address >> 24;
        switch (page) {
        case 0x02: return *reinterpret_cast<T *>(&mainRAM[address & 0x3FFFFF]);
        case 0x03: return *reinterpret_cast<T *>(&sharedWRAM[address & 0x7FFF]);
        case 0x04: return MMIORead<T>(address);
        case 0x06: return *reinterpret_cast<T *>(&vram[address & 0x1FFFFF]);
        default: return 0;
        }
    }

    template <typename T>
    void Write(uint32_t address, T value) {
        auto page = address >> 24;
        switch (page) {
        case 0x02: *reinterpret_cast<T *>(&mainRAM[address & 0x3FFFFF]) = value; break;
        case 0x03: *reinterpret_cast<T *>(&sharedWRAM[address & 0x7FFF]) = value; break;
        case 0x04: MMIOWrite<T>(address, value); break;
        case 0x06: *reinterpret_cast<T *>(&vram[address & 0x1FFFFF]) = value; break;
        }
    }

    bool vblank = false;
    int vblankCount = 0;
    uint16_t buttons = 0x03FF;

    template <typename T>
    T MMIORead(uint32_t address) {
        // Fake VBLANK counter
        if (address == 0x4000004) {
            ++vblankCount;
            if (vblankCount == 560190) {
                vblankCount = 0;
                vblank ^= true;
            }
            return vblank;
        }
        if (address == 0x4000130) {
            return buttons;
        }
        return 0;
    }

    template <typename T>
    void MMIOWrite(uint32_t address, T value) {
        // Not needed
    }
};

void printState(armajitto::arm::State &state) {
    printf("Registers in current mode:\n");
    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            int index = i * 4 + j;
            if (index >= 4 && index < 10) {
                printf("   R%d", index);
            } else {
                printf("  R%d", index);
            }
            printf(" = %08X", state.GPR(static_cast<armajitto::arm::GPR>(index)));
        }
        printf("\n");
    }

    auto printPSR = [](armajitto::arm::PSR &psr, const char *name) {
        auto flag = [](bool set, char c) { return (set ? c : '.'); };

        printf("%s = %08X   ", name, psr.u32);
        switch (psr.mode) {
        case armajitto::arm::Mode::User: printf("USR"); break;
        case armajitto::arm::Mode::FIQ: printf("FIQ"); break;
        case armajitto::arm::Mode::IRQ: printf("IRQ"); break;
        case armajitto::arm::Mode::Supervisor: printf("SVC"); break;
        case armajitto::arm::Mode::Abort: printf("ABT"); break;
        case armajitto::arm::Mode::Undefined: printf("UND"); break;
        case armajitto::arm::Mode::System: printf("SYS"); break;
        default: printf("%02Xh", static_cast<uint8_t>(psr.mode)); break;
        }
        if (psr.t) {
            printf("  THUMB  ");
        } else {
            printf("   ARM   ");
        }
        printf("%c%c%c%c%c%c%c\n", flag(psr.n, 'N'), flag(psr.z, 'Z'), flag(psr.c, 'C'), flag(psr.v, 'V'),
               flag(psr.q, 'Q'), flag(psr.i, 'I'), flag(psr.f, 'F'));
    };

    printPSR(state.CPSR(), "CPSR");
    for (auto mode : {armajitto::arm::Mode::FIQ, armajitto::arm::Mode::IRQ, armajitto::arm::Mode::Supervisor,
                      armajitto::arm::Mode::Abort, armajitto::arm::Mode::Undefined}) {
        auto spsrName = std::format("SPSR_{}", armajitto::arm::ToString(mode));
        printPSR(state.SPSR(mode), spsrName.c_str());
    }
    printf("\nBanked registers:\n");
    printf("usr              svc              abt              und              irq              fiq\n");
    for (int i = 0; i <= 15; i++) {
        auto printReg = [&](armajitto::arm::Mode mode) {
            if (mode == armajitto::arm::Mode::User || (i >= 13 && i <= 14) ||
                (mode == armajitto::arm::Mode::FIQ && i >= 8 && i <= 12)) {
                const auto gpr = static_cast<armajitto::arm::GPR>(i);
                if (i < 10) {
                    printf(" R%d = ", i);
                } else {
                    printf("R%d = ", i);
                }
                printf("%08X", state.GPR(gpr, mode));
            } else {
                printf("              ");
            }

            if (mode != armajitto::arm::Mode::FIQ) {
                printf("   ");
            } else {
                printf("\n");
            }
        };

        printReg(armajitto::arm::Mode::User);
        printReg(armajitto::arm::Mode::Supervisor);
        printReg(armajitto::arm::Mode::Abort);
        printReg(armajitto::arm::Mode::Undefined);
        printReg(armajitto::arm::Mode::IRQ);
        printReg(armajitto::arm::Mode::FIQ);
    }
    printf("Execution state: ");
    switch (state.ExecutionState()) {
    case armajitto::arm::ExecState::Running: printf("Running\n"); break;
    case armajitto::arm::ExecState::Halted: printf("Halted\n"); break;
    case armajitto::arm::ExecState::Stopped: printf("Stopped\n"); break;
    default: printf("Unknown (0x%X)\n", static_cast<uint8_t>(state.ExecutionState())); break;
    }
};

void testNDS() {
    auto sys = std::make_unique<MinimalNDSSystem>();

    struct CodeDesc {
        uint32_t romOffset;
        uint32_t entrypoint;
        uint32_t loadAddress;
        uint32_t size;
    } codeDesc;

    {
        std::ifstream ifsROM{"rockwrestler.nds", std::ios::binary};
        // std::ifstream ifsROM{"armwrestler.nds", std::ios::binary};
        if (!ifsROM) {
            printf("Could not open armwrestler.nds\n");
            return;
        }

        // 0x20: offset to ARM9 code in ROM file
        // 0x24: ARM9 entry point address
        // 0x28: offset in memory to load ARM9 code
        // 0x2C: size of ARM9 code
        ifsROM.seekg(0x20, std::ios::beg);
        ifsROM.read((char *)&codeDesc, sizeof(codeDesc));

        uint8_t *code = new uint8_t[codeDesc.size];
        ifsROM.seekg(codeDesc.romOffset, std::ios::beg);
        ifsROM.read((char *)code, codeDesc.size);
        sys->CopyToRAM(codeDesc.loadAddress, code, codeDesc.size);
        delete[] code;
    }

    // Create recompiler for ARM946E-S
    armajitto::Recompiler jit{{
        .system = *sys,
        .model = armajitto::CPUModel::ARM946ES,
    }};
    auto &armState = jit.GetARMState();

    // Configure CP15
    // These specs match the NDS's ARM946E-S
    auto &cp15 = armState.GetSystemControlCoprocessor();
    cp15.ConfigureTCM({.itcmSize = 0x8000, .dtcmSize = 0x4000});
    cp15.ConfigureCache({
        .type = armajitto::arm::cp15::cache::Type::WriteBackReg7CleanLockdownB,
        .separateCodeDataCaches = true,
        .code =
            {
                .size = 0x2000,
                .lineLength = armajitto::arm::cp15::cache::LineLength::_32B,
                .associativity = armajitto::arm::cp15::cache::Associativity::_4WayOr6Way,
            },
        .data =
            {
                .size = 0x1000,
                .lineLength = armajitto::arm::cp15::cache::LineLength::_32B,
                .associativity = armajitto::arm::cp15::cache::Associativity::_4WayOr6Way,
            },
    });

    // Start execution at the specified address and execution state
    armState.SetMode(armajitto::arm::Mode::System);
    armState.JumpTo(codeDesc.entrypoint, false);

    // Setup direct boot
    armState.GPR(armajitto::arm::GPR::R12) = codeDesc.entrypoint;
    armState.GPR(armajitto::arm::GPR::LR) = codeDesc.entrypoint;
    armState.GPR(armajitto::arm::GPR::PC) = codeDesc.entrypoint + 2 * sizeof(uint32_t);
    armState.GPR(armajitto::arm::GPR::SP) = 0x3002F7C;
    armState.GPR(armajitto::arm::GPR::SP, armajitto::arm::Mode::IRQ) = 0x3003F80;
    armState.GPR(armajitto::arm::GPR::SP, armajitto::arm::Mode::Supervisor) = 0x3003FC0;
    cp15.StoreRegister(0x0910, 0x0300000A);
    cp15.StoreRegister(0x0911, 0x00000020);
    cp15.StoreRegister(0x0100, cp15.LoadRegister(0x0100) | 0x00050000);

    // auto &optParams = jit.GetOptimizationParameters();
    // optParams.passes.constantPropagation = false;
    // optParams.passes.deadRegisterStoreElimination = false;
    // optParams.passes.deadGPRStoreElimination = false;
    // optParams.passes.deadHostFlagStoreElimination = false;
    // optParams.passes.deadFlagValueStoreElimination = false;
    // optParams.passes.deadVariableStoreElimination = false;
    // optParams.passes.bitwiseOpsCoalescence = false;
    // optParams.passes.arithmeticOpsCoalescence = false;
    // optParams.passes.hostFlagsOpsCoalescence = false;

    using namespace std::chrono_literals;

    bool running = true;
    std::jthread emuThread{[&] {
        using clk = std::chrono::steady_clock;

        auto t = clk::now();
        uint32_t frames = 0;
        uint64_t cycles = 0;
        uint64_t totalFrames = 0;
        while (running) {
            // Run for a full frame, assuming each instruction takes 3 cycles to complete
            cycles += jit.Run(560190 / 3);
            ++frames;
            ++totalFrames;
            /*if (totalFrames >= 15u && totalFrames < 30u) {
                sys->buttons &= ~(1 << 6);
            } else if (totalFrames >= 30u && totalFrames < 45u) {
                sys->buttons |= (1 << 6);
            } else if (totalFrames >= 45u && totalFrames < 60u) {
                sys->buttons &= ~(1 << 7);
            } else if (totalFrames >= 60u && totalFrames < 75u) {
                sys->buttons |= (1 << 7);
            }*/
            auto t2 = clk::now();
            if (t2 - t >= 1s) {
                printf("%u fps, %llu cycles\n", frames, cycles);
                frames = 0;
                cycles = 0;
                t = t2;
            }
        }
    }};

    SDL_Init(SDL_INIT_VIDEO);
    auto window = SDL_CreateWindow("[REDACTED]", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 256 * 2, 192 * 2,
                                   SDL_WINDOW_ALLOW_HIGHDPI);

    auto renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    auto texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR555, SDL_TEXTUREACCESS_STREAMING, 256, 192);

    while (running) {
        SDL_UpdateTexture(texture, nullptr, sys->vram.data(), sizeof(uint16_t) * 256);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        auto evt = SDL_Event{};
        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_QUIT) {
                running = false;
                break;
            }

            if (evt.type == SDL_KEYUP || evt.type == SDL_KEYDOWN) {
                auto bit = -1;
                bool down = evt.type == SDL_KEYDOWN;

                switch (reinterpret_cast<SDL_KeyboardEvent *>(&evt)->keysym.sym) {
                case SDLK_c: bit = 0; break;
                case SDLK_x: bit = 1; break;
                case SDLK_RSHIFT: bit = 2; break;
                case SDLK_RETURN: bit = 3; break;
                case SDLK_RIGHT: bit = 4; break;
                case SDLK_LEFT: bit = 5; break;
                case SDLK_UP: bit = 6; break;
                case SDLK_DOWN: bit = 7; break;
                case SDLK_f: bit = 8; break;
                case SDLK_a: bit = 9; break;
                }

                if (bit != -1) {
                    if (down) {
                        sys->buttons &= ~(1 << bit);
                    } else {
                        sys->buttons |= (1 << bit);
                    }
                }
            }
        }
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

int main(int argc, char *argv[]) {
    printf("armajitto %s\n\n", armajitto::version::name);

    testNDS();

    return EXIT_SUCCESS;
}
