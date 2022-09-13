#include <armajitto/armajitto.hpp>

#include <algorithm>

#include "interp.hpp"
#include "system.hpp"

int main(int argc, char *argv[]) {
    using namespace armajitto;

    int offset = 0;
    uint32_t limit = 0x20;
    if (argc >= 2) {
        offset = std::clamp(atoi(argv[1]), 0, 0x20);
        limit = 1;
    }

    FuzzerSystem interpSys;
    FuzzerSystem jitSys;

    auto interp = MakeARM946ESInterpreter(interpSys);

    Specification spec{jitSys, CPUModel::ARM946ES};
    Recompiler jit{spec};
    jit.GetOptions().translator.maxBlockSize = 1;

    auto &jitState = jit.GetARMState();

    auto &cp15 = jitState.GetSystemControlCoprocessor();
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

    auto printPSR = [](arm::PSR psr, const char *name) {
        auto flag = [](bool set, char c) { return (set ? c : '.'); };

        printf("%s = %08X   ", name, psr.u32);
        switch (psr.mode) {
        case arm::Mode::User: printf("USR"); break;
        case arm::Mode::FIQ: printf("FIQ"); break;
        case arm::Mode::IRQ: printf("IRQ"); break;
        case arm::Mode::Supervisor: printf("SVC"); break;
        case arm::Mode::Abort: printf("ABT"); break;
        case arm::Mode::Undefined: printf("UND"); break;
        case arm::Mode::System: printf("SYS"); break;
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

    auto printStates = [&] {
        printf("Interpreter state\n");
        for (int j = 0; j < 4; j++) {
            for (int i = 0; i < 4; i++) {
                int index = i * 4 + j;
                if (index >= 4 && index < 10) {
                    printf("   R%d", index);
                } else {
                    printf("  R%d", index);
                }
                printf(" = %08X", interp->GPR(static_cast<arm::GPR>(index)));
            }
            printf("\n");
        }
        printPSR({.u32 = interp->GetCPSR()}, "CPSR");
        printf("Memory:\n");
        for (int i = 0; i < 16; i++) {
            printf("%02X |", i * 16);
            for (int j = 0; j < 16; j++) {
                printf(" %02X", interpSys.mem[j + i * 16]);
            }
            printf("\n");
        }
        printf("Code memory:\n");
        for (int i = 0; i < 16; i++) {
            printf("%02X |", i * 16);
            for (int j = 0; j < 16; j++) {
                printf(" %02X", interpSys.codemem[j + i * 16]);
            }
            printf("\n");
        }

        printf("------------------------------------------------------------------------\n");

        printf("JIT state\n");
        for (int j = 0; j < 4; j++) {
            for (int i = 0; i < 4; i++) {
                int index = i * 4 + j;
                if (index >= 4 && index < 10) {
                    printf("   R%d", index);
                } else {
                    printf("  R%d", index);
                }
                printf(" = %08X", jitState.GPR(static_cast<arm::GPR>(index)));
            }
            printf("\n");
        }
        printPSR(jitState.CPSR(), "CPSR");
        printf("Memory:\n");
        for (int i = 0; i < 16; i++) {
            printf("%02X |", i * 16);
            for (int j = 0; j < 16; j++) {
                printf(" %02X", jitSys.mem[j + i * 16]);
            }
            printf("\n");
        }
        printf("Code memory:\n");
        for (int i = 0; i < 16; i++) {
            printf("%02X |", i * 16);
            for (int j = 0; j < 16; j++) {
                printf(" %02X", jitSys.codemem[j + i * 16]);
            }
            printf("\n");
        }
    };

    auto compareStates = [&](auto errorAction) {
        bool anyMismatch = false;

        auto setMismatch = [&] {
            if (!anyMismatch) {
                errorAction();
                anyMismatch = true;
            }
        };

        for (int i = 0; i < 16; i++) {
            auto interpReg = interp->GPR(static_cast<arm::GPR>(i));
            auto jitReg = jitState.GPR(static_cast<arm::GPR>(i));
            if (interpReg != jitReg) {
                setMismatch();
                printf("    R%d: expected %08X  !=  actual %08X\n", i, interpReg, jitReg);
            }
        }

        {
            auto interpCPSR = interp->GetCPSR();
            auto jitCPSR = jitState.CPSR().u32;
            if (interpCPSR != jitCPSR) {
                setMismatch();
                printf("    CPSR: expected %08X  !=  actual %08X\n", interpCPSR, jitCPSR);
            }
        }

        for (int i = 0; i < 256; i++) {
            auto interpMem = interpSys.mem[i];
            auto jitMem = jitSys.mem[i];
            if (interpMem != jitMem) {
                setMismatch();
                printf("    Memory [%02X]: expected %02X  !=  actual %02X\n", i, interpMem, jitMem);
            }
        }

        for (int i = 0; i < 256; i++) {
            auto interpMem = interpSys.codemem[i];
            auto jitMem = jitSys.codemem[i];
            if (interpMem != jitMem) {
                setMismatch();
                printf("    Code memory [%02X]: expected %02X  !=  actual %02X\n", i, interpMem, jitMem);
            }
        }

        /*if (anyMismatch) {
            printf("\n");
            printf("========================================================\n");
            printStates();
            printf("========================================================\n");
        }*/
    };

    auto init = [&](arm::Mode mode, uint32_t address, bool thumb) {
        // Set CPSR to the specified mode with I and F set, Thumb mode and all flags cleared
        const uint32_t cpsr = 0x000000C0 | static_cast<uint32_t>(mode) | (thumb << 5);
        interp->SetCPSR(cpsr);
        jitState.CPSR().u32 = cpsr;

        // Set all SPSRs to point back to System mode
        const uint32_t sysCPSR = 0x000000C0 | static_cast<uint32_t>(arm::Mode::System) | (thumb << 5);
        interp->SetSPSR(arm::Mode::FIQ, sysCPSR);
        interp->SetSPSR(arm::Mode::IRQ, sysCPSR);
        interp->SetSPSR(arm::Mode::Supervisor, sysCPSR);
        interp->SetSPSR(arm::Mode::Abort, sysCPSR);
        interp->SetSPSR(arm::Mode::Undefined, sysCPSR);
        jitState.SPSR(arm::Mode::FIQ).u32 = sysCPSR;
        jitState.SPSR(arm::Mode::IRQ).u32 = sysCPSR;
        jitState.SPSR(arm::Mode::Supervisor).u32 = sysCPSR;
        jitState.SPSR(arm::Mode::Abort).u32 = sysCPSR;
        jitState.SPSR(arm::Mode::Undefined).u32 = sysCPSR;

        // Setup GPRs to a recognizable pattern
        for (uint32_t reg = 0; reg < 15; reg++) {
            auto gpr = static_cast<arm::GPR>(reg);
            const uint32_t regVal = (0xFF - reg) | (reg << 8);
            interp->GPR(gpr, arm::Mode::System) = regVal;
            jitState.GPR(gpr, arm::Mode::System) = regVal;
            if (reg >= 8 && reg <= 12) {
                interp->GPR(gpr, arm::Mode::FIQ) = regVal | 0x10000;
                jitState.GPR(gpr, arm::Mode::FIQ) = regVal | 0x10000;
            }
            if (reg >= 13 && reg <= 14) {
                interp->GPR(gpr, arm::Mode::FIQ) = regVal | 0x10000;
                jitState.GPR(gpr, arm::Mode::FIQ) = regVal | 0x10000;

                interp->GPR(gpr, arm::Mode::Supervisor) = regVal | 0x20000;
                jitState.GPR(gpr, arm::Mode::Supervisor) = regVal | 0x20000;

                interp->GPR(gpr, arm::Mode::Abort) = regVal | 0x30000;
                jitState.GPR(gpr, arm::Mode::Abort) = regVal | 0x30000;

                interp->GPR(gpr, arm::Mode::IRQ) = regVal | 0x40000;
                jitState.GPR(gpr, arm::Mode::IRQ) = regVal | 0x40000;

                interp->GPR(gpr, arm::Mode::Undefined) = regVal | 0x50000;
                jitState.GPR(gpr, arm::Mode::Undefined) = regVal | 0x50000;
            }
        }

        // Jump to the specified address
        interp->JumpTo(address, thumb);
        jitState.JumpTo(address, thumb);
    };

    // Test *all* Thumb instructions in selected modes.
    // These modes differ in the banked registers used:
    // - System uses all base registers
    // - IRQ has its own R13 and R14
    // - FIQ has its own R8 through R14
    /*for (auto mode : {arm::Mode::System, arm::Mode::IRQ, arm::Mode::FIQ}) {
        printf("===============================\n");
        printf("Testing mode %d\n\n", mode);
        init(mode, 0x10000, true);
        printStates();
        printf("\n");

        for (uint32_t instr = 0; instr <= 0xFFFF; instr++) {
            // Reset interpreter and JIT
            interp->Reset();
            jit.Reset();

            // Reset system memory
            interpSys.Reset();
            jitSys.Reset();

            // Write Thumb instruction to code memory
            *reinterpret_cast<uint16_t *>(&interpSys.codemem[0]) = instr;
            *reinterpret_cast<uint16_t *>(&jitSys.codemem[0]) = instr;

            init(mode, 0x10000, true);

            // Run both the interpreter and the JIT for one instruction
            interp->Run(1);
            jit.Run(1);

            // Compare states and print any discrepancies
            compareStates([&] { printf("[!] Discrepancies found on mode %d, instruction %04X\n", mode, instr); });
        }
    }*/

    // Test *all* ARM instructions with AL and NV conditions in selected modes.
    // These modes differ in the banked registers used:
    // - System uses all base registers
    // - IRQ has its own R13 and R14
    // - FIQ has its own R8 through R14
    for (auto mode : {arm::Mode::System, arm::Mode::IRQ, arm::Mode::FIQ}) {
        printf("===============================\n");
        printf("Testing mode %d\n\n", mode);
        init(mode, 0x10000, false);
        printStates();
        printf("\n");
        fprintf(stderr, "Testing mode %d\n", mode);

#ifdef _DEBUG
        const uint32_t testInstr = 0xE1CF0080;
        const uint32_t start = testInstr - 0xE0000000;
        const uint32_t end = start /*+ 0x1000000*/ + 1;
#else
        const uint32_t start = offset * 0x1000000;
        const uint32_t end = start + limit * 0x1000000;
#endif
        for (uint32_t i = start; i < end; i++) {
            const uint32_t instr = i + 0xE0000000;
            if ((instr & 0xFFFFF) == 0) {
                fprintf(stderr, "  Instructions %08X to %08X\n", instr, instr + 0xFFFFF);
            }

            // Reset interpreter and JIT
            interp->Reset();
            jit.Reset();

            // Reset system memory
            interpSys.Reset();
            jitSys.Reset();

            // Write ARM instruction to code memory
            *reinterpret_cast<uint32_t *>(&interpSys.codemem[0]) = instr;
            *reinterpret_cast<uint32_t *>(&jitSys.codemem[0]) = instr;

            init(mode, 0x10000, false);

            // Run both the interpreter and the JIT for one instruction
            interp->Run(1);
            jit.Run(1);

            // Compare states and print any discrepancies
            compareStates([&] { printf("[!] Discrepancies found on mode %d, instruction %08X\n", mode, instr); });
        }
    }

    return 0;
}
