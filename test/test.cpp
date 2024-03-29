#include <armajitto/armajitto.hpp>

#include <SDL2/SDL.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>

class MinimalGBASystem : public armajitto::ISystem {
public:
    MinimalGBASystem() {
        bios.fill(0);
        ewram.fill(0);
        iwram.fill(0);
        pram.fill(0);
        vram.fill(0);
        rom.fill(0xFF);

        using MemArea = armajitto::MemoryArea;
        using MemAttr = armajitto::MemoryAttributes;

        m_memMap.Map(MemArea::AllRead, 0, 0x0000000, 0x4000, MemAttr::RXC, bios.data(), bios.size());
        m_memMap.Map(MemArea::All, 0, 0x2000000, 0x40000, MemAttr::RWX, ewram.data(), ewram.size());
        m_memMap.Map(MemArea::All, 0, 0x3000000, 0x8000, MemAttr::RWX, iwram.data(), iwram.size());
        // m_memMap.Map(MemArea::All, 0, 0x5000000, 0x200, MemAttr::RWX, pram.data(), pram.size());
        m_memMap.Map(MemArea::All, 0, 0x6000000, 0x18000, MemAttr::RWX, vram.data(), vram.size());
        m_memMap.Map(MemArea::AllRead, 0, 0x8000000, 0x2000000, MemAttr::RXC, rom.data(), rom.size());
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

    template <typename T>
    T Read(uint32_t address) {
        auto page = address >> 24;
        switch (page) {
        case 0x00: return *reinterpret_cast<T *>(&bios[address & 0x3FFF]);
        case 0x02: return *reinterpret_cast<T *>(&ewram[address & 0x3FFFF]);
        case 0x03: return *reinterpret_cast<T *>(&iwram[address & 0x7FFF]);
        case 0x04: return MMIORead<T>(address);
        case 0x05: return *reinterpret_cast<T *>(&pram[address & 0x1FF]);
        case 0x06: return *reinterpret_cast<T *>(&vram[address % 0x18000]);
        case 0x08: return *reinterpret_cast<T *>(&rom[address & 0x1FFFFFF]);
        default: return 0;
        }
    }

    template <typename T>
    void Write(uint32_t address, T value) {
        auto page = address >> 24;
        switch (page) {
        case 0x02: *reinterpret_cast<T *>(&ewram[address & 0x3FFFF]) = value; break;
        case 0x03: *reinterpret_cast<T *>(&iwram[address & 0x7FFF]) = value; break;
        case 0x04: MMIOWrite<T>(address, value); break;
        case 0x05: *reinterpret_cast<T *>(&pram[address & 0x1FF]) = value; break;
        case 0x06: *reinterpret_cast<T *>(&vram[address % 0x18000]) = value; break;
        case 0x08: *reinterpret_cast<T *>(&rom[address & 0x1FFFFFF]) = value; break;
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
            if (vblankCount == 280896) {
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

    std::array<uint8_t, 0x4000> bios;
    std::array<uint8_t, 0x40000> ewram;
    std::array<uint8_t, 0x8000> iwram;
    std::array<uint8_t, 0x200> pram;
    std::array<uint8_t, 0x18000> vram;
    std::array<uint8_t, 0x2000000> rom;
};

class MinimalNDSSystem : public armajitto::ISystem {
public:
    MinimalNDSSystem() {
        mainRAM.fill(0);
        sharedWRAM.fill(0);
        vram.fill(0);

        using MemArea = armajitto::MemoryArea;
        using MemAttr = armajitto::MemoryAttributes;

        m_memMap.Map(MemArea::All, 0, 0x2000000, 0x1000000, MemAttr::RWX, mainRAM.data(), mainRAM.size());
        m_memMap.Map(MemArea::All, 0, 0x3000000, 0x1000000, MemAttr::RWX, sharedWRAM.data(), sharedWRAM.size());
        m_memMap.Map(MemArea::All, 0, 0x6800000, 0xA4000, MemAttr::RWX, vram.data());
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

    template <typename T>
    T Read(uint32_t address) {
        auto page = address >> 24;
        switch (page) {
        case 0x02: return *reinterpret_cast<T *>(&mainRAM[address & 0x3FFFFF]);
        case 0x03: return *reinterpret_cast<T *>(&sharedWRAM[address & 0x7FFF]);
        case 0x04: return MMIORead<T>(address);
        case 0x06:
            if ((address & 0x1FFFFFF) < 0xA4000) {
                return *reinterpret_cast<T *>(&vram[address & 0x1FFFFF]);
            } else {
                return 0;
            }
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
        case 0x06:
            if ((address & 0x1FFFFFF) < 0xA4000) {
                *reinterpret_cast<T *>(&vram[address & 0x1FFFFF]) = value;
            }
            break;
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

    std::array<uint8_t, 0x400000> mainRAM;
    std::array<uint8_t, 0x8000> sharedWRAM;
    std::array<uint8_t, 0xA4000> vram;
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
        auto spsrName = std::string("SPSR_") + armajitto::arm::ToString(mode);
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

void testGBA() {
    auto sys = std::make_unique<MinimalGBASystem>();

    {
        std::ifstream ifsBIOS{"gba_bios.bin", std::ios::binary};
        if (!ifsBIOS) {
            printf("Could not open gba_bios.bin\n");
            return;
        }

        ifsBIOS.seekg(0, std::ios::end);
        auto size = ifsBIOS.tellg();
        ifsBIOS.seekg(0, std::ios::beg);
        ifsBIOS.read((char *)sys->bios.data(), size);
    }
    {
        // std::ifstream ifsROM{"c:/temp/jsmolka/arm.gba", std::ios::binary};
        // std::ifstream ifsROM{"c:/temp/jsmolka/thumb.gba", std::ios::binary};
        // std::ifstream ifsROM{"c:/temp/FuzzARM/ARM_DataProcessing.gba", std::ios::binary};
        // std::ifstream ifsROM{"c:/temp/FuzzARM/ARM_Any.gba", std::ios::binary};
        // std::ifstream ifsROM{"c:/temp/FuzzARM/THUMB_DataProcessing.gba", std::ios::binary};
        // std::ifstream ifsROM{"c:/temp/FuzzARM/THUMB_Any.gba", std::ios::binary};
        // std::ifstream ifsROM{"c:/temp/FuzzARM/FuzzARM.gba", std::ios::binary};
        // std::ifstream ifsROM{"c:/temp/FuzzARM/100k-T1.gba", std::ios::binary};
        // std::ifstream ifsROM{"c:/temp/FuzzARM/100k-T2.gba", std::ios::binary};
        // std::ifstream ifsROM{"c:/temp/FuzzARM/100k-A1.gba", std::ios::binary};
        // std::ifstream ifsROM{"c:/temp/FuzzARM/100k-A2.gba", std::ios::binary};
        // std::ifstream ifsROM{"c:/temp/FuzzARM/250k-1.gba", std::ios::binary};
        std::ifstream ifsROM{"c:/temp/FuzzARM/250k-2.gba", std::ios::binary};
        if (!ifsROM) {
            printf("Could not open rom\n");
            return;
        }

        ifsROM.seekg(0, std::ios::end);
        auto size = ifsROM.tellg();
        ifsROM.seekg(0, std::ios::beg);
        ifsROM.read((char *)sys->rom.data(), size);
    }

    // Create recompiler for ARM7TDMI
    armajitto::Recompiler jit{{
        .system = *sys,
        .model = armajitto::CPUModel::ARM7TDMI,
    }};
    // jit.GetOptions().translator.maxBlockSize = 1;
    // jit.GetOptions().optimizer.passes.SetAll(false);
    // jit.GetOptions().compiler.enableBlockLinking = true;

    auto &armState = jit.GetARMState();

    // Start execution at the specified address and execution state
    armState.SetMode(armajitto::arm::Mode::System);
    armState.JumpTo(0x8000000, false);

    // Setup direct boot
    armState.GPR(armajitto::arm::GPR::SP) = 0x03007F00;
    armState.GPR(armajitto::arm::GPR::SP, armajitto::arm::Mode::IRQ) = 0x03007FA0;
    armState.GPR(armajitto::arm::GPR::SP, armajitto::arm::Mode::Supervisor) = 0x03007FE0;

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
            cycles += jit.Run(280896 / 3);
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
    auto window = SDL_CreateWindow("armajitto", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 240 * 2, 160 * 2,
                                   SDL_WINDOW_ALLOW_HIGHDPI);

    auto renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    auto texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR555, SDL_TEXTUREACCESS_STREAMING, 240, 160);
    uint16_t *texData = new uint16_t[240 * 160];

    while (running) {
        for (uint32_t i = 0; i < 240 * 160; i++) {
            uint8_t clr = sys->vram[i];
            uint16_t pal = *reinterpret_cast<uint16_t *>(&sys->pram[clr * 2]);
            texData[i] = pal;
        }
        SDL_UpdateTexture(texture, nullptr, texData, sizeof(uint16_t) * 240);
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

    delete[] texData;
}

void testNDS() {
    auto sys = std::make_unique<MinimalNDSSystem>();

    struct CodeDesc {
        uint32_t romOffset;
        uint32_t entrypoint;
        uint32_t loadAddress;
        uint32_t size;
    } codeDesc;

    {
        // std::ifstream ifsROM{"rockwrestler.nds", std::ios::binary};
        std::ifstream ifsROM{"armwrestler.nds", std::ios::binary};
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

    using namespace armajitto::arm::cp15;

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
                .lineLength = cache::LineLength::_32B,
                .associativity = cache::Associativity::_4Way,
            },
        .data =
            {
                .size = 0x1000,
                .lineLength = cache::LineLength::_32B,
                .associativity = cache::Associativity::_4Way,
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

    using CycleCountingMethod = armajitto::Options::Translator::CycleCountingMethod;
    auto &options = jit.GetOptions();
    // options.translator.cycleCountingMethod = CycleCountingMethod::InstructionFixed;
    // options.translator.cyclesPerInstruction = 2;
    options.translator.cycleCountingMethod = CycleCountingMethod::SubinstructionFixed;
    options.translator.cyclesPerMemoryAccess = 1;

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
    auto window = SDL_CreateWindow("armajitto", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 256 * 2, 192 * 2,
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

void testCompiler() {
    auto sys = std::make_unique<MinimalNDSSystem>();

    const uint32_t baseAddress = 0x2000000;

    bool thumb = false;
    uint64_t numInstrs = 0;
    auto writeARM = [&, address = baseAddress](uint32_t instr) mutable {
        *reinterpret_cast<uint32_t *>(&sys->mainRAM[address & 0x3FFFFF]) = instr;
        address += sizeof(instr);
        numInstrs++;
        thumb = false;
    };

    auto writeThumb = [&, address = baseAddress](uint16_t instr) mutable {
        *reinterpret_cast<uint16_t *>(&sys->mainRAM[address & 0x3FFFFF]) = instr;
        address += sizeof(instr);
        numInstrs++;
        thumb = true;
    };

    // Infinite optimizer loop
    /*writeARM(0xE590100C); // ldr r1, [r0, #0xC]
    writeARM(0xE3A02000); // mov r2, #0x0
    writeARM(0xE20114FF); // and r1, r1, #0xFF000000
    writeARM(0xE3C114FF); // bic r1, r1, #0xFF000000
    writeARM(0xE5802004); // str r2, [r0, #0x4]
    writeARM(0xE5802000); // str r2, [r0]
    writeARM(0xE5802008); // str r2, [r0, #0x8]
    writeARM(0xE580100C); // str r1, [r0, #0xC]
    writeARM(0xE12FFF1E); // bx lr*/

    // Unoptimized code (arithmetic ops coalescence)
    // writeARM(0xE59F00F4); // ldr r0, [pc, #0xF4]
    // writeARM(0xE2800DFF); // add r0, r0, #0x3FC0
    // writeARM(0xE2400040); // sub r0, r0, #0x40
    // writeARM(0xE240D004); // sub sp, r0, #0x4

    // -------------------------------------------------------------------------
    // Fuzzer detections

    // Thumb SUB with lhs=rhs
    // writeThumb(0x1A00); // subs r0, r0, r0
    // writeThumb(0x1A4A); // subs r2, r1, r1
    // writeThumb(0x1A91); // subs r1, r2, r2

    // Thumb CMP pc, <reg>
    // writeThumb(0x4587); // cmp pc, r0

    // Thumb add offset to SP
    // writeThumb(0xA800); // add r0, sp, #0

    // Thumb multiple load store
    // writeThumb(0xC000); // stm r0!, {}

    // Thumb long branch suffix
    // writeThumb(0xF800);

    // Thumb BLX (ARMv5)
    // writeThumb(0x47F0);

    // ARM LDM with user mode registers and SPSR->CPSR
    // writeARM(0xE87D8000); // ldmda sp!, {pc} ^
    // writeARM(0xE87D8001); // ldmda sp!, {r0, pc} ^

    // ARM LDR/STR with PC writeback
    // writeARM(0xE60F0001); // str r0, [pc], -r1

    // ARM LDRD with writeback to Rd
    // writeARM(0xE00000D0); // ldrd r0, r1, [r0], -r0
    // writeARM(0xE00000D1); // ldrd r0, r1, [r0], -r1

    // ARM TST with hidden PC argument
    // writeARM(0xE310F1AA); // tst r0, #0x8000002a
    // writeARM(0xE314F1F8); // tst r4, #248, #2
    // writeARM(0xE110F060); // tst r0, r0, rrx
    // writeARM(0xE310F102); // tst r0, #0x80000000

    // ARM MCR2
    // writeARM(0xFE000F10); // mcr2 p15, #0, r0, c0, c0, #0

    // ARM ALU ops with shift by PC
    // writeARM(0xE0000F31); // and r0, r0, r1, lsr pc

    // ARM ALU ops with PC as operand
    // writeARM(0xE00F0080); // and r0, pc, r0, lsl #1

    // writeThumb(0x40D4); // lsrs r4, r2

    // -------------------------------------------------------------------------
    // Detections in real code

    // Bad dead reg optimization
    /*writeARM(0xE59F10DC); // ldr r1, [pc, #0xDC]
    writeARM(0xE3A08000); // mov r8, #0x0
    writeARM(0xE5911000); // ldr r1, [r1]
    writeARM(0xE59F30D4); // ldr r3, [pc, #0xD4]
    writeARM(0xE59F20D4); // ldr r2, [pc, #0xD4]
    writeARM(0xE2811B27); // add r1, r1, #0x9C00
    writeARM(0xE0807108); // add r7, r0, r8, lsl #0x2
    writeARM(0xE1D1C0D0); // ldrsb r12, [r1]
    writeARM(0xE1D758D2); // ldrsb r5, [r7, #0x82]
    writeARM(0xE1D160D1); // ldrsb r6, [r1, #0x1]
    writeARM(0xE1D748D3); // ldrsb r4, [r7, #0x83]
    writeARM(0xE005059C); // mul r5, r12, r5
    writeARM(0xE00E0496); // mul lr, r6, r4
    writeARM(0xE0C64592); // smull r4, r6, r2, r5
    writeARM(0xE0856006); // add r6, r5, r6
    writeARM(0xE0C4CE92); // smull r12, r4, r2, lr
    writeARM(0xE08E4004); // add r4, lr, r4
    writeARM(0xE1A0C088); // mov r12, r8, lsl #0x1
    writeARM(0xE2888001); // add r8, r8, #0x1
    writeARM(0xE1D798D4); // ldrsb r9, [r7, #0x84]
    writeARM(0xE1A05FA5); // mov r5, r5, lsr #0x1F
    writeARM(0xE1A07FAE); // mov r7, lr, lsr #0x1F
    writeARM(0xE3580004); // cmp r8, #0x4
    writeARM(0xE1D1E0D2); // ldrsb lr, [r1, #0x2]
    writeARM(0xE1A06246); // mov r6, r6, asr #0x4
    writeARM(0xE0856006); // add r6, r5, r6
    writeARM(0xE009099E); // mul r9, lr, r9
    writeARM(0xE0CE5992); // smull r5, lr, r2, r9
    writeARM(0xE089E00E); // add lr, r9, lr
    writeARM(0xE1A05FA9); // mov r5, r9, lsr #0x1F
    writeARM(0xE1A0E24E); // mov lr, lr, asr #0x4
    writeARM(0xE085E00E); // add lr, r5, lr*/

    // Poor CPSR optimization
    /*writeThumb(0x880B); // ldrh r3, [r1]
    writeThumb(0x0704); // lsls r4, r0, #0x1C
    writeThumb(0x0EE4); // lsrs r4, r4, #0x1B
    writeThumb(0x5B2C); // ldrh r4, [r5, r4]
    writeThumb(0x0900); // lsrs r0, r0, #0x4
    writeThumb(0x4060); // eors r0, r4
    writeThumb(0x1C1C); // adds r4, r3, #0x0
    writeThumb(0x40D4); // lsrs r4, r2
    writeThumb(0x0724); // lsls r4, r4, #0x1C
    writeThumb(0x0EE4); // lsrs r4, r4, #0x1B
    writeThumb(0x5B2C); // ldrh r4, [r5, r4]
    writeThumb(0x3204); // adds r2, #0x4
    writeThumb(0x4060); // eors r0, r4
    writeThumb(0x2A10); // cmp r2, #0x10*/

    // Too aggressive CPSR optimization
    /*writeARM(0xE3E02102); // mov r2, #0x7FFFFFFF  (mvn r2, #0x80000000)
    writeARM(0xE3E03000); // mov r3, #0xFFFFFFFF  (mvn r3, #0x0)
    writeARM(0xE0921002); // adds r1, r2, r2   N..V
    // writeARM(0xE0921003); // adds r1, r2, r3   ..C.
    writeARM(0xE1020052); // qadd r0, r2, r2   Q
    // writeARM(0xE1030052); // qadd r0, r2, r3   no change*/

    // Bad bitwise ops coalescence due to instruction consuming multiple variables from different instructions
    /*writeThumb(0x25FF); // movs r5, #0xFF
    writeThumb(0x400D); // ands r5, r1
    writeThumb(0x022A); // lsls r2, r5, #0x8
    writeThumb(0x432A); // orrs r2, r5
    writeThumb(0x0415); // lsls r5, r2, #0x10
    // writeThumb(0x4315); // orrs r5, r2
    // writeThumb(0x2C0F); // cmp r4, #0xF*/

    // More advanced scenario for the bad bitwise ops coalescence
    /*writeThumb(0x26FF); // movs r6, #0xFF
    writeThumb(0x0136); // lsls r6, #0x4
    writeThumb(0x270F); // movs r7, #0xF
    writeThumb(0x433E); // orrs r7, r6
    writeThumb(0x25FF); // movs r5, #0xFF
    writeThumb(0x400D); // ands r5, r1
    writeThumb(0x4035); // ands r5, r7
    writeThumb(0x022A); // lsls r2, r5, #0x8
    writeThumb(0x432A); // orrs r2, r5
    writeThumb(0x4032); // ands r2, r6
    writeThumb(0x0415); // lsls r5, r2, #0x10*/

    // Another case of bad bitwise ops coalescence
    /*writeARM(0xE59D0008); // ldr r0, [sp, #8]
    writeARM(0xE5902000); // ldr r2, [r0]
    writeARM(0xE2020CFF); // and r0, r2, #0xff00
    writeARM(0xE1A00420); // lsr r0, r0, #8
    writeARM(0xE1A01220); // lsr r1, r0, #4
    writeARM(0xE58D0014); // str r0, [sp, #0x14]
    writeARM(0xE200300F); // and r3, r0, #0xf
    writeARM(0xE201000F); // and r0, r1, #0xf
    writeARM(0xE2121102); // ands r1, r2, #0x80000000*/

    // Bad arithmetic ops coalescence
    /*writeARM(0xE92D4000); // stmdb sp!, {lr}
    writeARM(0xE24DD004); // sub sp, sp, #0x4
    writeARM(0xE3A0E301); // mov lr, #0x4000000
    writeARM(0xE59EC000); // ldr r12, [lr]
    writeARM(0xE59F1028); // ldr r1, [pc, #0x28]
    writeARM(0xE20C2803); // and r2, r12, #0x30000
    writeARM(0xE3A03000); // mov r3, #0x0
    writeARM(0xE59F0020); // ldr r0, [pc, #0x20]
    writeARM(0xE1A02822); // mov r2, r2, lsr #0x10
    writeARM(0xE1C130B0); // strh r3, [r1]
    writeARM(0xE1C020B0); // strh r2, [r0]
    writeARM(0xE3CC0803); // bic r0, r12, #0x30000
    writeARM(0xE58E0000); // str r0, [lr]
    writeARM(0xE28DD004); // add sp, sp, #0x4
    writeARM(0xE8BD4000); // ldmia sp!, {lr}
    writeARM(0xE12FFF1E); // bx lr*/

    // Bad bitwise ops coalescence due to multiple rotations in a chain
    /*// writeThumb(0x0108); // lsls r0, r1, #0x4
    // writeThumb(0x4308); // orrs r0, r1
    writeThumb(0x0600); // lsls r0, r0, #0x18
    writeThumb(0x0E01); // lsrs r1, r0, #0x18
    writeThumb(0x060B); // lsls r3, r1, #0x18
    writeThumb(0x0408); // lsls r0, r1, #0x10
    // writeThumb(0x4318); // orrs r0, r3
    writeThumb(0x020D); // lsls r5, r1, #0x8
    // writeThumb(0x4328); // orrs r0, r5
    // writeThumb(0x4308); // orrs r0, r1
    writeThumb(0x68E1); // ldr r1, [r4, #0xC]
    // writeThumb(0x7A23); // ldrb r3, [r4, #0x8]
    // writeThumb(0x79E4); // ldrb r4, [r4, #0x7]
    // writeThumb(0x4362); // muls r2, r4
    // writeThumb(0x435A); // muls r2, r3
    // writeThumb(0xF0A9); // (blx prefix)
    // writeThumb(0xEEF0); // blx #0x20C4BBA*/

    // Bad variable lifetime optimization
    /*writeARM(0xE1010090); // swp r0, r0, [r1]
    writeARM(0xE12FFF1E); // bx lr*/

    // Bad bitwise ops coalescence
    // writeARM(0xE1A04A23); // lsr r4, r3, #0x14
    // writeARM(0xE08B4084); // add r4, r11, r4, lsl #1
    // writeARM(0xE1D440B0); // ldrh r4, [r4]
    // writeARM(0xE204500F); // and r5, r4, #0xF
    // writeARM(0xE1A04224); // lsr r4, r4, #4
    // writeARM(0xE204601F); // and r6, r4, #0x1F
    // writeARM(0xE1A042A4); // lsr r4, r4, #5
    // writeARM(0xE1B03513); // lsls r3, r3, r5

    // Infinite loop
    // writeARM(0xEAFFFFFE); // jmp (self)

    // Infinite loop with extra ops
    writeARM(0xE2800004); // add r0, r0, #4
    writeARM(0xEAFFFFFD); // jmp (beginning)

    using namespace armajitto;

    Recompiler jit{{
        .system = *sys,
        .model = CPUModel::ARM946ES,
    }};

    const uint32_t sysCPSR = 0x000000C0 | static_cast<uint32_t>(arm::Mode::System) | (thumb << 5);
    auto &armState = jit.GetARMState();
    armState.CPSR().mode = arm::Mode::IRQ;
    armState.SPSR(arm::Mode::FIQ).u32 = sysCPSR;
    armState.SPSR(arm::Mode::IRQ).u32 = sysCPSR;
    armState.SPSR(arm::Mode::Supervisor).u32 = sysCPSR;
    armState.SPSR(arm::Mode::Abort).u32 = sysCPSR;
    armState.SPSR(arm::Mode::Undefined).u32 = sysCPSR;
    armState.JumpTo(baseAddress, thumb);

    // jit.GetOptions().optimizer.passes.constantPropagation = false;
    // jit.GetOptions().optimizer.passes.varLifetimeOptimization = false;

    /*for (uint32_t reg = 0; reg < 15; reg++) {
        auto gpr = static_cast<arm::GPR>(reg);
        const uint32_t regVal = (0xFF - reg) | (reg << 8);
        armState.GPR(gpr, arm::Mode::System) = regVal;
        if (reg >= 8 && reg <= 12) {
            armState.GPR(gpr, arm::Mode::FIQ) = regVal | 0x10000;
        }
        if (reg >= 13 && reg <= 14) {
            armState.GPR(gpr, arm::Mode::FIQ) = regVal | 0x10000;
            armState.GPR(gpr, arm::Mode::Supervisor) = regVal | 0x20000;
            armState.GPR(gpr, arm::Mode::Abort) = regVal | 0x30000;
            armState.GPR(gpr, arm::Mode::IRQ) = regVal | 0x40000;
            armState.GPR(gpr, arm::Mode::Undefined) = regVal | 0x50000;
        }
    }*/
    armState.GPR(arm::GPR::R2) = 0;
    armState.GPR(arm::GPR::R4) = 6;

    jit.GetOptions().translator.maxBlockSize = numInstrs;
    auto cyclesRun = jit.Run(numInstrs);
    printf("%llu cycles executed\n", cyclesRun);
}

int main(int argc, char *argv[]) {
    printf("armajitto %s\n\n", armajitto::version::name);

    // testGBA();
    // testNDS();
    testCompiler();

    return EXIT_SUCCESS;
}
