#include <armajitto/armajitto.hpp>

#include "interp.hpp"
#include "system.hpp"

int main() {
    using namespace armajitto;

    FuzzerSystem interpSys;
    FuzzerSystem jitSys;

    auto interp = MakeARM946ESInterpreter(interpSys);

    Specification spec{jitSys, CPUModel::ARM946ES};
    Recompiler jit{spec};
    jit.GetTranslatorParameters().maxBlockSize = 1;

    auto &jitState = jit.GetARMState();

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

    auto compareStates = [&] {
        bool anyMismatch = false;
        for (int i = 0; i < 16; i++) {
            auto interpReg = interp->GPR(static_cast<arm::GPR>(i));
            auto jitReg = jitState.GPR(static_cast<arm::GPR>(i));
            if (interpReg != jitReg) {
                printf("[!] Register mismatch: R%d = %08X  !=  %08X\n", i, interpReg, jitReg);
                anyMismatch = true;
            }
        }

        {
            auto interpCPSR = interp->GetCPSR();
            auto jitCPSR = jitState.CPSR().u32;
            if (interpCPSR != jitCPSR) {
                printf("[!] CPSR mismatch: %08X  !=  %08X\n", interpCPSR, jitCPSR);
                anyMismatch = true;
            }
        }

        for (int i = 0; i < 256; i++) {
            auto interpMem = interpSys.mem[i];
            auto jitMem = jitSys.mem[i];
            if (interpMem != jitMem) {
                printf("[!] Memory mismatch: [%02X] %08X  !=  %08X\n", i, interpMem, jitMem);
                anyMismatch = true;
            }
        }

        for (int i = 0; i < 256; i++) {
            auto interpMem = interpSys.codemem[i];
            auto jitMem = jitSys.codemem[i];
            if (interpMem != jitMem) {
                printf("[!] Code memory mismatch: [%02X] %08X  !=  %08X\n", i, interpMem, jitMem);
                anyMismatch = true;
            }
        }

        if (anyMismatch) {
            printf("\n");
            printf("========================================================\n");
            printStates();
            printf("========================================================\n");
        }
    };

    // Test *all* Thumb instructions in selected modes.
    // These modes differ in the banked registers used:
    // - System uses all base registers
    // - IRQ has its own R13 and R14
    // - FIQ has its own R8 through R14
    for (auto mode : {arm::Mode::System, arm::Mode::IRQ, arm::Mode::FIQ}) {
        for (uint32_t instr = 0; instr <= 0xFFFF; instr++) {
            // Reset interpreter and JIT
            interp->Reset();
            jit.Reset();

            // Reset system memory
            interpSys.Reset();
            jitSys.Reset();

            // Set CPSR to the specified mode with I and F set, Thumb mode and all flags cleared
            const uint32_t cpsr = 0x000000E0 | static_cast<uint32_t>(mode);
            interp->SetCPSR(cpsr);
            jitState.CPSR().u32 = cpsr;

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
            interp->JumpTo(0x10000, false);
            jitState.JumpTo(0x10000, false);

            // Write Thumb instruction to code memory
            *reinterpret_cast<uint16_t *>(&interpSys.codemem[0]) = instr;
            *reinterpret_cast<uint16_t *>(&jitSys.codemem[0]) = instr;

            // Run both the interpreter and the JIT for one instruction
            interp->Run(1);
            jit.Run(1);

            // Compare states and print any discrepancies
            compareStates();
        }
    }

    return 0;
}
