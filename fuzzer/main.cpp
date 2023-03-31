#include <armajitto/armajitto.hpp>

#include <algorithm>
#include <random>

#include "interp.hpp"
#include "system.hpp"

using namespace armajitto;

void printPSR(arm::PSR psr, const char *name) {
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
    printf("%c%c%c%c%c%c%c\n", flag(psr.n, 'N'), flag(psr.z, 'Z'), flag(psr.c, 'C'), flag(psr.v, 'V'), flag(psr.q, 'Q'),
           flag(psr.i, 'I'), flag(psr.f, 'F'));
}

void printInterpState(Interpreter &interp, FuzzerSystem &sys, bool printMemory) {
    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            int index = i * 4 + j;
            if (index >= 4 && index < 10) {
                printf("   R%d", index);
            } else {
                printf("  R%d", index);
            }
            printf(" = %08X", interp.GPR(static_cast<arm::GPR>(index)));
        }
        printf("\n");
    }
    printPSR({.u32 = interp.GetCPSR()}, "CPSR");
    printPSR({.u32 = interp.GetSPSR()}, "SPSR");
    if (printMemory) {
        printf("Memory:\n");
        for (int i = 0; i < 16; i++) {
            printf("  %02X |", i * 16);
            for (int j = 0; j < 16; j++) {
                printf(" %02X", sys.mem[j + i * 16]);
            }
            printf("\n");
        }
        printf("Code memory:\n");
        for (int i = 0; i < 16; i++) {
            printf("  %02X |", i * 16);
            for (int j = 0; j < 16; j++) {
                printf(" %02X", sys.codemem[j + i * 16]);
            }
            printf("\n");
        }
    }
}

void printJITState(arm::State &state, FuzzerSystem &sys, bool printMemory) {
    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            int index = i * 4 + j;
            if (index >= 4 && index < 10) {
                printf("   R%d", index);
            } else {
                printf("  R%d", index);
            }
            printf(" = %08X", state.GPR(static_cast<arm::GPR>(index)));
        }
        printf("\n");
    }
    printPSR(state.CPSR(), "CPSR");
    printPSR(state.SPSR(), "SPSR");
    if (printMemory) {
        printf("Memory:\n");
        for (int i = 0; i < 16; i++) {
            printf("  %02X |", i * 16);
            for (int j = 0; j < 16; j++) {
                printf(" %02X", sys.mem[j + i * 16]);
            }
            printf("\n");
        }
        printf("Code memory:\n");
        for (int i = 0; i < 16; i++) {
            printf("  %02X |", i * 16);
            for (int j = 0; j < 16; j++) {
                printf(" %02X", sys.codemem[j + i * 16]);
            }
            printf("\n");
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------

void initInterp(Interpreter &interp, arm::Mode mode, uint32_t address, bool thumb) {
    // Set CPSR to the specified mode with I and F set, Thumb mode and all flags cleared
    const uint32_t cpsr = 0x000000C0 | static_cast<uint32_t>(mode) | (thumb << 5);
    interp.SetCPSR(cpsr);

    // Set all SPSRs to point back to System mode
    const uint32_t sysCPSR = 0x000000C0 | static_cast<uint32_t>(arm::Mode::System) | (thumb << 5);
    interp.SetSPSR(arm::Mode::FIQ, sysCPSR);
    interp.SetSPSR(arm::Mode::IRQ, sysCPSR);
    interp.SetSPSR(arm::Mode::Supervisor, sysCPSR);
    interp.SetSPSR(arm::Mode::Abort, sysCPSR);
    interp.SetSPSR(arm::Mode::Undefined, sysCPSR);

    // Setup GPRs to a recognizable pattern
    for (uint32_t reg = 0; reg < 15; reg++) {
        auto gpr = static_cast<arm::GPR>(reg);
        const uint32_t regVal = (0xFF - reg) | (reg << 8);
        interp.GPR(gpr, arm::Mode::System) = regVal;
        if (reg >= 8 && reg <= 12) {
            interp.GPR(gpr, arm::Mode::FIQ) = regVal | 0x10000;
        }
        if (reg >= 13 && reg <= 14) {
            interp.GPR(gpr, arm::Mode::FIQ) = regVal | 0x10000;
            interp.GPR(gpr, arm::Mode::Supervisor) = regVal | 0x20000;
            interp.GPR(gpr, arm::Mode::Abort) = regVal | 0x30000;
            interp.GPR(gpr, arm::Mode::IRQ) = regVal | 0x40000;
            interp.GPR(gpr, arm::Mode::Undefined) = regVal | 0x50000;
        }
    }

    // Jump to the specified address
    interp.JumpTo(address, thumb);
}

void initJIT(arm::State &state, arm::Mode mode, uint32_t address, bool thumb) {
    // Set CPSR to the specified mode with I and F set, Thumb mode and all flags cleared
    const uint32_t cpsr = 0x000000C0 | static_cast<uint32_t>(mode) | (thumb << 5);
    state.CPSR().u32 = cpsr;

    // Set all SPSRs to point back to System mode
    const uint32_t sysCPSR = 0x000000C0 | static_cast<uint32_t>(arm::Mode::System) | (thumb << 5);
    state.SPSR(arm::Mode::FIQ).u32 = sysCPSR;
    state.SPSR(arm::Mode::IRQ).u32 = sysCPSR;
    state.SPSR(arm::Mode::Supervisor).u32 = sysCPSR;
    state.SPSR(arm::Mode::Abort).u32 = sysCPSR;
    state.SPSR(arm::Mode::Undefined).u32 = sysCPSR;

    // Setup GPRs to a recognizable pattern
    for (uint32_t reg = 0; reg < 15; reg++) {
        auto gpr = static_cast<arm::GPR>(reg);
        const uint32_t regVal = (0xFF - reg) | (reg << 8);
        state.GPR(gpr, arm::Mode::System) = regVal;
        if (reg >= 8 && reg <= 12) {
            state.GPR(gpr, arm::Mode::FIQ) = regVal | 0x10000;
        }
        if (reg >= 13 && reg <= 14) {
            state.GPR(gpr, arm::Mode::FIQ) = regVal | 0x10000;
            state.GPR(gpr, arm::Mode::Supervisor) = regVal | 0x20000;
            state.GPR(gpr, arm::Mode::Abort) = regVal | 0x30000;
            state.GPR(gpr, arm::Mode::IRQ) = regVal | 0x40000;
            state.GPR(gpr, arm::Mode::Undefined) = regVal | 0x50000;
        }
    }

    // Jump to the specified address
    state.JumpTo(address, thumb);
};

// ---------------------------------------------------------------------------------------------------------------------

void interpVsJITFuzzer(uint32_t offset, uint32_t limit) {
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

    auto printStates = [&](bool printMemory) {
        printf("Interpreter state\n");
        printInterpState(*interp, interpSys, printMemory);

        printf("------------------------------------------------------------------------\n");

        printf("JIT state\n");
        printJITState(jitState, jitSys, printMemory);
        printf("\n");
    };

    auto compareStates = [&](bool printMismatch, auto errorAction) {
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

        {
            auto interpSPSR = interp->GetSPSR();
            auto jitSPSR = jitState.SPSR().u32;
            if (interpSPSR != jitSPSR) {
                setMismatch();
                printf("    SPSR: expected %08X  !=  actual %08X\n", interpSPSR, jitSPSR);
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

        if (anyMismatch && printMismatch) {
            printf("\n");
            printf("========================================================\n");
            printStates(true);
            printf("========================================================\n");
        }
    };

    auto init = [&](arm::Mode mode, uint32_t address, bool thumb) {
        initInterp(*interp, mode, address, thumb);
        initJIT(jitState, mode, address, thumb);
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
        printStates(false);
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
            compareStates(false, [&] { printf("[!] Discrepancies found on mode %d, instruction %04X\n", mode,
    instr);
    });
        }
    }*/

    // Test *all* ARM instructions with AL and NV conditions in selected modes.
    // These modes differ in the banked registers used:
    // - System uses all base registers
    // - IRQ has its own R13 and R14
    // - FIQ has its own R8 through R14
    /*for (auto mode : {arm::Mode::System, arm::Mode::IRQ, arm::Mode::FIQ}) {
        printf("===============================\n");
        printf("Testing mode %d\n\n", mode);
        init(mode, 0x10000, false);
        printStates(false);
        printf("\n");
        fprintf(stderr, "Testing mode %d\n", mode);

#ifdef _DEBUG
        const uint32_t testInstr = 0xE1CF0080;
        const uint32_t start = testInstr - 0xE0000000;
        // const uint32_t end = start + 1;
        const uint32_t end = start + 0x1000000 + 1;
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
            compareStates(false,
                          [&] { printf("[!] Discrepancies found on mode %d, instruction %08X\n", mode, instr); });
        }
    }*/

    // Test random sequences of ARM instructions in selected modes.
    // The entire code memory is filled with random bytes and both the interpreter and JIT run for 64 cycles.
    // These modes differ in the banked registers used:
    // - System uses all base registers
    // - IRQ has its own R13 and R14
    // - FIQ has its own R8 through R14
    /*std::default_random_engine generator;
    std::uniform_int_distribution<uint32_t> distribution{};

    const uint64_t numIters = 100;
    static constexpr auto blockSize = jitSys.codemem.size() / sizeof(uint32_t);

    // Precompute random data to be reused across different modes
    std::vector<uint32_t> randomData{};
    randomData.resize(numIters * blockSize);
    std::generate(randomData.begin(), randomData.end(), [&] { return distribution(generator); });

    // Configure the JIT
    jit.GetOptions().translator.maxBlockSize = blockSize;
    jit.GetOptions().compiler.enableBlockLinking = true;

    for (auto mode : {arm::Mode::System, arm::Mode::IRQ, arm::Mode::FIQ}) {
        printf("===============================\n");
        printf("Testing mode %d\n\n", mode);
        init(mode, 0x10000, true);
        // printStates(false);
        printf("\n");

        for (uint64_t iteration = 0; iteration < numIters; iteration++) {
            // Reset interpreter and JIT
            interp->Reset();
            jit.Reset();

            // Reset system memory
            interpSys.Reset();
            jitSys.Reset();

            // Fill code memory with random data
            std::copy_n(&randomData[iteration * blockSize], blockSize,
                        reinterpret_cast<uint32_t *>(&jitSys.codemem[0]));
            interpSys.codemem = jitSys.codemem;

            init(mode, 0x10000, false);

            // Run both the interpreter and the JIT for 64 cycles
            auto cyclesExecuted = jit.Run(64);
            interp->Run(cyclesExecuted);

            // Compare states and print any discrepancies
            compareStates(true, [&] { printf("[!] Discrepancies found on mode %d\n", mode); });
        }
    }*/

    // -----------------------------------------------------------------------------------------------------------------

    // Test a fixed sequence of ARM instructions in selected modes.
    // These modes differ in the banked registers used:
    // - System uses all base registers
    // - IRQ has its own R13 and R14
    // - FIQ has its own R8 through R14

    std::vector<uint8_t> code;
    uint32_t numInstrs = 0;
    auto writeInstr = [&](uint32_t instr) {
        code.push_back(instr >> 0);
        code.push_back(instr >> 8);
        code.push_back(instr >> 16);
        code.push_back(instr >> 24);
        ++numInstrs;
    };

    // writeInstr(0xE3B004DE); // movs r0, #0xDE000000
    // writeInstr(0x039008AD); // orrseq r0, #0x00AD0000
    // writeInstr(0x03900CBE); // orrseq r0, #0x0000BE00
    // writeInstr(0x039000EF); // orrseq r0, #0x000000EF
    // writeInstr(0xE2801008); // add r1, r0, #0x8
    // writeInstr(0xE01120D1); // ldrsb r2, [r1], -r1
    // writeInstr(0xE19230F2); // ldrsh r3, [r2, r2]
    // writeInstr(0xE3A04000); // mov r4, #0x0
    // writeInstr(0xE18030B4); // strh r3, [r0, r4]
    // writeInstr(0xE114F253); // tst r4, r3, asr r2  (with hidden PC)
    // writeInstr(0x297040FC); // ldmdbcs r0!, {r2-r7, lr}^
    // writeInstr(0xEF060000); // swi #0x60000

    // writeInstr(0xE92D500F); // stmdb sp!, {r0-r3, r12, lr}
    // writeInstr(0xEE190F11); // mrc p15, 0, r0, c9, c1, 0
    // writeInstr(0xE1A00620); // mov r0, r0, lsr #0xC
    // writeInstr(0xE1A00600); // mov r0, r0, lsl #0xC
    // writeInstr(0xE2800C40); // add r0, r0, #0x4000
    // writeInstr(0xE28FE000); // add lr, pc, #0x0
    // writeInstr(0xE510F004); // ldr pc, [r0, #-0x4]

    // writeInstr(0xE8BD4004); // ldmia sp!, {r2, lr}
    // writeInstr(0xE3A0C0D3); // mov r12, #0xD3
    // writeInstr(0xE12FF00C); // msr cpsr_fsxc, r12
    // writeInstr(0xE8BD0800); // ldmia sp!, {r11}
    // writeInstr(0xE16FF00B); // msr spsr_fsxc, r11
    // writeInstr(0xE8BD5800); // ldmia sp!, {r11, r12, lr}
    // writeInstr(0xE1B0F00E); // movs pc, lr

    /*numInstrs = 64;
    code.insert(code.end(),
                {
                    0xA7, 0x52, 0x0A, 0xD3, 0x0F, 0x8B, 0x37, 0x88, 0x12, 0xD4, 0x8F, 0xFB, 0x27, 0x70, 0x41, 0xE6,
                    0xA7, 0x95, 0xF1, 0xBA, 0xCD, 0xD5, 0xDC, 0xC5, 0xDA, 0x52, 0x08, 0x58, 0x6E, 0x98, 0x29, 0x34,
                    0x46, 0x91, 0x85, 0x95, 0x4A, 0x45, 0x38, 0xE8, 0x04, 0xC0, 0x96, 0x1B, 0x4D, 0xB7, 0xF3, 0x5F,
                    0x9C, 0xCF, 0x03, 0xE8, 0x75, 0x5A, 0x1C, 0xD9, 0x96, 0xFC, 0x30, 0xE1, 0xD0, 0x8F, 0xA2, 0x53,
                    0x94, 0xC1, 0x58, 0xD1, 0x85, 0x03, 0xA5, 0x53, 0xF5, 0x11, 0xBF, 0x42, 0xF5, 0x06, 0xC2, 0xA8,
                    0x23, 0xBB, 0x27, 0x98, 0xD9, 0x3A, 0x36, 0x96, 0xA9, 0x62, 0xC3, 0x05, 0xC4, 0x29, 0x8A, 0x0C,
                    0x78, 0xCB, 0xDD, 0x6C, 0x48, 0x78, 0x1C, 0xC5, 0x5A, 0x58, 0x0E, 0x50, 0x64, 0x78, 0xE3, 0x7D,
                    0x6E, 0x10, 0x57, 0x29, 0xAB, 0xA0, 0xF9, 0x7C, 0xE9, 0x9E, 0xC3, 0x2D, 0x2B, 0x3D, 0xDB, 0x6D,
                    0x98, 0x3C, 0x42, 0x6C, 0x24, 0xE0, 0x0C, 0x65, 0xEB, 0x69, 0x1F, 0x18, 0x24, 0xAD, 0xC5, 0xC6,
                    0xC1, 0xD8, 0x38, 0x99, 0xE3, 0x80, 0x89, 0xF9, 0xE8, 0x7D, 0x8E, 0x78, 0xBC, 0x08, 0x9E, 0xBF,
                    0xE9, 0xBB, 0x29, 0xB2, 0x2B, 0xB9, 0x6E, 0xA2, 0xAC, 0xD9, 0x2B, 0xB3, 0xCA, 0x67, 0x7A, 0x12,
                    0x62, 0xC0, 0x76, 0xA3, 0xD6, 0x18, 0x24, 0xA2, 0xD0, 0x42, 0x9A, 0x08, 0xB3, 0x57, 0x67, 0x87,
                    0xDA, 0x46, 0x9D, 0x11, 0xA8, 0xA4, 0xD1, 0x4B, 0xD4, 0x49, 0xD1, 0x51, 0xEE, 0xAF, 0x48, 0x53,
                    0xAF, 0xB8, 0xE6, 0x87, 0x4A, 0x13, 0x91, 0xA1, 0xF8, 0xC0, 0x89, 0xA7, 0x49, 0xE8, 0x97, 0xFE,
                    0x5C, 0xBB, 0x59, 0x68, 0xDB, 0xB5, 0x1B, 0x84, 0x06, 0x4A, 0xEA, 0xD1, 0xCA, 0xCF, 0x75, 0xB9,
                    0x3F, 0x5F, 0xE6, 0xB7, 0x98, 0xFC, 0xDD, 0x3E, 0x15, 0x67, 0xF9, 0xF7, 0x20, 0x2D, 0x14, 0x55,
                });*/

    /*writeInstr(0xE3A01001); // mov r1, #1
    writeInstr(0x13A02002); // movne r2, #2
    writeInstr(0x43A03003); // movmi r3, #3    -- should pass
    writeInstr(0x03A04004); // moveq r4, #4    -- should fail
    */

    writeInstr(0xE3E02102); // mov r2, #0x7FFFFFFF  (mvn r2, #0x80000000)
    writeInstr(0xE3E03000); // mov r3, #0xFFFFFFFF  (mvn r3, #0x0)
    writeInstr(0xE0921002); // adds r1, r2, r2   N..V
    // writeInstr(0xE0921003); // adds r1, r2, r3   ..C.
    // writeInstr(0xE1020052); // qadd r0, r2, r2   Q
    writeInstr(0xE1030052); // qadd r0, r2, r3   no change

    // Configure the JIT
    jit.GetOptions().translator.maxBlockSize = numInstrs;
    jit.GetOptions().compiler.enableBlockLinking = true;
    jit.GetOptions().optimizer.passes.SetAll(true);
    jit.GetOptions().translator.cycleCountingMethod =
        armajitto::Options::Translator::CycleCountingMethod::InstructionFixed;
    jit.GetOptions().translator.cyclesPerInstruction = 1;

    // Reset interpreter and JIT
    interp->Reset();
    jit.Reset();

    // Reset system memory
    interpSys.Reset();
    jitSys.Reset();

    // Fill code memory
    std::copy(code.begin(), code.end(), jitSys.codemem.begin());
    interpSys.codemem = jitSys.codemem;

    init(arm::Mode::System, 0x10000, false);

    // Disable IRQs
    jitState.CPSR().i = 1;
    interp->SetCPSR(interp->GetCPSR() | (1 << 7));

    // Run both the interpreter and the JIT
    auto cyclesExecuted = jit.Run(1);
    interp->Run(cyclesExecuted);

    printStates(true);
    printf("%llu cycles executed\n\n", cyclesExecuted);

    // Compare states and print any discrepancies
    compareStates(false, [&] { printf("[!] Discrepancies found\n"); });

    /*for (auto mode : {arm::Mode::System, arm::Mode::IRQ, arm::Mode::FIQ}) {
        printf("===============================\n");
        printf("Testing mode %d\n\n", mode);
        printf("\n");

        // Reset interpreter and JIT
        interp->Reset();
        jit.Reset();

        // Reset system memory
        interpSys.Reset();
        jitSys.Reset();

        // Fill code memory
        std::copy(code.begin(), code.end(), jitSys.codemem.begin());
        interpSys.codemem = jitSys.codemem;

        init(mode, 0x10000, false);

        // Enable IRQs
        jitState.CPSR().i = 0;
        interp->SetCPSR(interp->GetCPSR() & ~(1 << 7));

        // Setup flags
        jitState.CPSR().n = 1;
        jitState.CPSR().z = 0;
        jitState.CPSR().c = 0;
        jitState.CPSR().v = 0;

        interp->SetCPSR((interp->GetCPSR() | 0x80000000) & ~0x70000000);
        interp->SetSPSR(armajitto::arm::Mode::IRQ,
                        (interp->GetSPSR(armajitto::arm::Mode::IRQ) | 0x40000000) & ~0xC0000000);

        // Expected outcomes of each iteration:
        // 0 = run mov r1, #1
        // 1 = enter IRQ
        // 2 = exit IRQ
        // 3 = run movne r2, #3   (condition passes)
        // 4 = run movmi r3, #3   (condition passes)
        // 5 = skip moveq r4, #4  (condition fails)
        // for (int iter = 0; iter < 6; iter++) {
        //     // Assert IRQ lines on an specific iteration
        //     bool assertIRQ = (iter == 1);
        //     jitState.IRQLine() = assertIRQ;
        //     interp->IRQLine() = assertIRQ;
        //
        //     // Run both the interpreter and the JIT
        //     auto cyclesExecuted = jit.Run(1);
        //     interp->Run(cyclesExecuted);
        //
        //     printStates(true);
        //     printf("%llu cycles executed\n\n", cyclesExecuted);
        //
        //     // Compare states and print any discrepancies
        //     compareStates(false, [&] { printf("[!] Discrepancies found on mode %d, iteration %d\n", mode, iter); });
        // }

        // Deassert IRQ lines
        jitState.IRQLine() = false;
        interp->IRQLine() = false;

        // Run both the interpreter and the JIT for one cycle
        // Should execute mov r1, #1
        {
            auto cyclesExecuted = jit.Run(1);
            interp->Run(cyclesExecuted);
        }
        printf("\n========================================================\n");

        // Assert IRQ lines
        jitState.IRQLine() = true;
        interp->IRQLine() = true;

        // Run both the interpreter and the JIT for one cycle
        // Should enter IRQ handler
        {
            auto cyclesExecuted = jit.Run(1);
            interp->Run(cyclesExecuted);
        }
        printf("\n========================================================\n");

        // Deassert IRQ lines
        jitState.IRQLine() = false;
        interp->IRQLine() = false;

        // Run both the interpreter and the JIT for one cycle
        // Should exit IRQ handler
        {
            auto cyclesExecuted = jit.Run(1);
            interp->Run(cyclesExecuted);
        }
        printf("\n========================================================\n");

        // Assert IRQ lines
        jitState.IRQLine() = true;
        interp->IRQLine() = true;

        // Run both the interpreter and the JIT for one cycle
        // Should enter IRQ handler again
        {
            auto cyclesExecuted = jit.Run(1);
            interp->Run(cyclesExecuted);
        }
        printf("\n========================================================\n");

        // Deassert IRQ lines
        jitState.IRQLine() = false;
        interp->IRQLine() = false;

        // Run both the interpreter and the JIT for four cycles
        // Should exit IRQ handler, then execute the three next instructions:
        //   movne r2, #3   (condition passes)
        //   movmi r3, #3   (condition passes)
        //   moveq r4, #4   (condition fails)
        {
            auto cyclesExecuted = jit.Run(5);
            interp->Run(cyclesExecuted);
        }

        printStates(true);

        // Compare states and print any discrepancies
        compareStates(false, [&] { printf("[!] Discrepancies found on mode %d\n", mode); });
    }*/
}

void DualJITFuzzer(uint32_t offset, uint32_t limit) {
    FuzzerSystem jit1Sys;
    Specification unoptSpec{jit1Sys, CPUModel::ARM946ES};
    Recompiler jit1{unoptSpec};
    auto &jit1State = jit1.GetARMState();

    FuzzerSystem jit2Sys;
    Specification optSpec{jit2Sys, CPUModel::ARM946ES};
    Recompiler jit2{optSpec};
    auto &jit2State = jit2.GetARMState();

    auto setupJIT = [](arm::State &state) {
        auto &cp15 = state.GetSystemControlCoprocessor();
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
    };

    setupJIT(jit1State);
    setupJIT(jit2State);

    jit1.GetOptions().optimizer.passes.SetAll(false);
    jit2.GetOptions().optimizer.passes.SetAll(true);

    auto printStates = [&](bool printMemory) {
        printf("JIT 1 state\n");
        printJITState(jit1State, jit1Sys, printMemory);
        printf("------------------------------------------------------------------------\n");
        printf("JIT 2 state\n");
        printJITState(jit2State, jit2Sys, printMemory);
        printf("\n");
    };

    auto compareStates = [&](bool printMismatch, auto errorAction) {
        bool anyMismatch = false;

        auto setMismatch = [&] {
            if (!anyMismatch) {
                errorAction();
                anyMismatch = true;
            }
        };

        for (int i = 0; i < 16; i++) {
            auto jit1Reg = jit1State.GPR(static_cast<arm::GPR>(i));
            auto jit2Reg = jit2State.GPR(static_cast<arm::GPR>(i));
            if (jit1Reg != jit2Reg) {
                setMismatch();
                printf("    R%d: %08X  !=  %08X\n", i, jit1Reg, jit2Reg);
            }
        }

        {
            auto jit1CPSR = jit1State.CPSR().u32;
            auto jit2CPSR = jit2State.CPSR().u32;
            if (jit1CPSR != jit2CPSR) {
                setMismatch();
                printf("    CPSR: %08X  !=  %08X\n", jit1CPSR, jit2CPSR);
            }
        }

        {
            auto jit1SPSR = jit1State.SPSR().u32;
            auto jit2SPSR = jit2State.SPSR().u32;
            if (jit1SPSR != jit2SPSR) {
                setMismatch();
                printf("    SPSR: %08X  !=  %08X\n", jit1SPSR, jit2SPSR);
            }
        }

        for (int i = 0; i < 256; i++) {
            auto jit1Mem = jit1Sys.mem[i];
            auto jit2Mem = jit2Sys.mem[i];
            if (jit1Mem != jit2Mem) {
                setMismatch();
                printf("    Memory [%02X]: %02X  !=  %02X\n", i, jit1Mem, jit2Mem);
            }
        }

        for (int i = 0; i < 256; i++) {
            auto jit1CodeMem = jit1Sys.codemem[i];
            auto jit2CodeMem = jit2Sys.codemem[i];
            if (jit1CodeMem != jit2CodeMem) {
                setMismatch();
                printf("    Code memory [%02X]: %02X  !=  %02X\n", i, jit1CodeMem, jit2CodeMem);
            }
        }

        if (anyMismatch && printMismatch) {
            printf("\n");
            printf("========================================================\n");
            printStates(true);
            printf("========================================================\n");
        }
    };

    auto init = [&](arm::Mode mode, uint32_t address, bool thumb) {
        initJIT(jit1State, mode, address, thumb);
        initJIT(jit2State, mode, address, thumb);
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
        printStates(false);
        printf("\n");
        fprintf(stderr, "Testing mode %d\n", mode);

        for (uint32_t instr = 0; instr <= 0xFFFF; instr++) {
            if ((instr & 0xFFF) == 0) {
                fprintf(stderr, "  Instructions %04X to %04X\n", instr, instr + 0xFFF);
            }

            // Reset both JITs
            jit1.Reset();
            jit2.Reset();

            // Reset system memory
            jit1Sys.Reset();
            jit2Sys.Reset();

            // Write Thumb instruction to code memory
            *reinterpret_cast<uint16_t *>(&jit1Sys.codemem[0]) = instr;
            *reinterpret_cast<uint16_t *>(&jit2Sys.codemem[0]) = instr;

            init(mode, 0x10000, true);

            // Run both the interpreter and the JIT for one instruction
            jit1.Run(1);
            jit2.Run(1);

            // Compare states and print any discrepancies
            compareStates(false,
                          [&] { printf("[!] Discrepancies found on mode %d, instruction %04X\n", mode, instr); });
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
        printStates(false);
        printf("\n");
        fprintf(stderr, "Testing mode %d\n", mode);

#ifdef _DEBUG
        const uint32_t testInstr = 0xE0210110;
        const uint32_t start = testInstr - 0xE0000000;
        const uint32_t end = start + 1;
        // const uint32_t end = start + 0x1000000 + 1;
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
            jit1.Reset();
            jit2.Reset();

            // Reset system memory
            jit1Sys.Reset();
            jit2Sys.Reset();

            // Write ARM instruction to code memory
            *reinterpret_cast<uint32_t *>(&jit1Sys.codemem[0]) = instr;
            *reinterpret_cast<uint32_t *>(&jit2Sys.codemem[0]) = instr;

            init(mode, 0x10000, false);

            // Run both the interpreter and the JIT for one instruction
            jit1.Run(1);
            jit2.Run(1);

            // Compare states and print any discrepancies
            compareStates(false,
                          [&] { printf("[!] Discrepancies found on mode %d, instruction %08X\n", mode, instr); });
        }
    }
}

int main(int argc, char *argv[]) {
    {
        int offset = 0;
        uint32_t limit = 0x20;
        if (argc >= 2) {
            offset = std::clamp(atoi(argv[1]), 0, 0x20);
            limit = 1;
        }
        interpVsJITFuzzer(offset, limit);
    }

    /*{
        int offset = 0;
        uint32_t limit = 0x20;
        if (argc >= 2) {
            offset = std::clamp(atoi(argv[1]), 0, 0x20);
            limit = 1;
        }
        DualJITFuzzer(offset, limit);
    }*/

    return 0;
}
