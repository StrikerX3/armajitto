#include <armajitto/armajitto.hpp>
#include <armajitto/host/x86_64/cpuid.hpp>
#include <armajitto/ir/optimizer.hpp>
#include <armajitto/ir/translator.hpp>

#include <array>
#include <cstdio>

class System : public armajitto::ISystem {
public:
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

    void ROMWriteByte(uint32_t address, uint8_t value) {
        rom[address & 0xFFF] = value;
    }

    void ROMWriteHalf(uint32_t address, uint16_t value) {
        *reinterpret_cast<uint16_t *>(&rom[address & 0xFFE]) = value;
    }

    void ROMWriteWord(uint32_t address, uint32_t value) {
        *reinterpret_cast<uint32_t *>(&rom[address & 0xFFC]) = value;
    }

private:
    // Memory map:
    //   ROM   0x0000..0x0FFF
    //   RAM   0x1000..0x1FFF
    //   MMIO  0x2000..0x2FFF
    //   open  ...every other address
    std::array<uint8_t, 0x1000> rom;
    std::array<uint8_t, 0x1000> ram;

    template <typename T>
    T Read(uint32_t address) {
        auto page = address >> 12;
        switch (page) {
        case 0x0: return *reinterpret_cast<T *>(&rom[address & 0xFFF]);
        case 0x1: return *reinterpret_cast<T *>(&ram[address & 0xFFF]);
        case 0x2: return MMIORead<T>(address);
        default: return 0;
        }
    }

    template <typename T>
    void Write(uint32_t address, T value) {
        auto page = address >> 12;
        switch (page) {
        case 0x1: *reinterpret_cast<T *>(&ram[address & 0xFFF]) = value; break;
        case 0x2: MMIOWrite(address, value); break;
        }
    }

    template <typename T>
    T MMIORead(uint32_t address) {
        // TODO: implement some basic hardware emulation for MMIO tests
        return 0;
    }

    template <typename T>
    void MMIOWrite(uint32_t address, T value) {
        // TODO: implement some basic hardware emulation for MMIO tests
    }
};

void testBasic() {
    // System implements the armajitto::ISystem interface
    System sys{};

    // Fill in the ROM with some code
    bool thumb = false;
    sys.ROMWriteWord(0x0100, 0xE3A00012); // mov r0, #0x12
    sys.ROMWriteWord(0x0104, 0xE3801B0D); // orr r1, r0, #0x3400
    sys.ROMWriteWord(0x0108, 0xEAFFFFFC); // b #0

    // Define a specification for the recompiler
    armajitto::Specification spec{
        .system = sys,
        .arch = armajitto::CPUArch::ARMv5TE,
    };

    // Make a recompiler from the specification
    armajitto::Recompiler jit{spec};

    // Get the ARM state -- registers, coprocessors, etc.
    auto &armState = jit.GetARMState();

    // Start execution at the specified address and execution state
    armState.JumpTo(0x0100, thumb);
    // The above is equivalent to:
    // armState.GPR(15) = 0x0100 + (thumb ? 4 : 8);
    // armState.CPSR().t = thumb;

    printf("PC = %08X  T = %u\n", armState.GPR(15), armState.CPSR().t);

    // Run for at least 32 cycles
    uint64_t cyclesExecuted = jit.Run(32);
    printf("Executed %llu cycles\n", cyclesExecuted);

    /*
    // Raise the IRQ line
    sys.IRQLine() = true;
    // Interrupts are handled in Run()

    // Switch to FIQ mode (also switches banked registers and updates I and F flags)
    armState.SetMode(armajitto::arm::Mode::FIQ);
    */
}

void testCPUID() {
    auto &cpuid = armajitto::x86_64::CPUID::Instance();
    if (cpuid.HasBMI2()) {
        printf("BMI2 available\n");
    }
    if (cpuid.HasLZCNT()) {
        printf("LZCNT available\n");
    }
    if (cpuid.HasFastPDEPAndPEXT()) {
        printf("Fast PDEP/PEXT available\n");
    }
}
void testTranslator() {
    System sys{};

    const uint32_t baseAddress = 0x0100;

    uint32_t address = baseAddress;
    bool thumb = false;

    [[maybe_unused]] auto writeThumb = [&](uint16_t opcode) {
        sys.ROMWriteHalf(address, opcode);
        address += sizeof(opcode);
        thumb = true;
    };

    [[maybe_unused]] auto writeARM = [&](uint32_t opcode) {
        sys.ROMWriteWord(address, opcode);
        address += sizeof(opcode);
        thumb = false;
    };

    // ARM branches
    // writeARM(0xE16F2F13); // clz r2, r3
    // writeARM(0xEAFFFFFE); // b $
    // writeARM(0xEBFFFFFE); // bl $
    // writeARM(0xFAFFFFFE); // blx $
    // writeARM(0xE12FFF11); // bx r1
    // writeARM(0xE12FFF31); // blx r1

    // Thumb branches
    // writeThumb(0xF7FF); // bl $ (prefix)
    // writeThumb(0xFFFE); // bl $ (suffix)
    // writeThumb(0xF7FF); // blx $ (prefix)
    // writeThumb(0xEFFE); // blx $ (suffix)
    // writeThumb(0xD0FE); // beq $
    // writeThumb(0xE7FE); // b $
    // writeThumb(0x4708); // bx r1
    // writeThumb(0x4788); // blx r1

    // ARM ALU operations
    writeARM(0xE3A02012); // mov r2, #0x12
    writeARM(0xE3A03B0D); // mov r3, #0x3400
    writeARM(0xE3A04004); // mov r4, #0x4
    writeARM(0xE0121003); // ands r1, r2, r3
    writeARM(0xE0321383); // eors r1, r2, r3, lsl #7
    writeARM(0xE0521413); // subs r1, r2, r3, lsl r4
    writeARM(0xE07213A3); // rsbs r1, r2, r3, lsr #7
    writeARM(0xE0921433); // adds r1, r2, r3, lsr r4
    writeARM(0xE0B213C3); // adcs r1, r2, r3, asr #7
    writeARM(0xE0D21453); // sbcs r1, r2, r3, asr r4
    writeARM(0xE0F213E3); // rscs r1, r2, r3, ror #7
    writeARM(0xE1120003); // tst r2, r3
    writeARM(0xE1320003); // teq r2, r3
    writeARM(0xE1520003); // cmp r2, r3
    writeARM(0xE1720003); // cmn r2, r3
    writeARM(0xE1921473); // orrs r1, r2, r3, ror r4
    writeARM(0xE1B01002); // movs r1, r2
    writeARM(0xE1D21063); // bics r1, r2, r3, rrx
    writeARM(0xE1E01003); // mvn r1, r3
    writeARM(0xEAFFFFFE); // b $

    // ARM ALU comparisons
    // writeARM(0xE0021003); // and r1, r2, r3
    // writeARM(0xE1120003); // tst r2, r3
    // writeARM(0xE0221003); // eor r1, r2, r3
    // writeARM(0xE1320003); // teq r2, r3
    // writeARM(0xE0421003); // sub r1, r2, r3
    // writeARM(0xE1520003); // cmp r2, r3
    // writeARM(0xE0821003); // add r1, r2, r3
    // writeARM(0xE1720003); // cmn r2, r3
    // writeARM(0xEAFFFFFE); // b $

    // QADD, QSUB, QDADD, QDSUB
    // writeARM(0xE1031052); // qadd r1, r2, r3
    // writeARM(0xE1231052); // qsub r1, r2, r3
    // writeARM(0xE1431052); // qdadd r1, r2, r3
    // writeARM(0xE1631052); // qdsub r1, r2, r3
    // writeARM(0xEAFFFFFE); // b $

    // MUL, MLA
    // writeARM(0xE0110392); // muls r1, r2, r3
    // writeARM(0xE0314392); // mlas r1, r2, r3, r4
    // writeARM(0xE0010392); // mul r1, r2, r3
    // writeARM(0xE0214392); // mla r1, r2, r3, r4
    // writeARM(0xEAFFFFFE); // b $

    // UMULL, UMLAL, SMULL, SMLAL
    // writeARM(0xE0821493); // umull r1, r2, r3, r4
    // writeARM(0xE0C21493); // smull r1, r2, r3, r4
    // writeARM(0xE0A21493); // umlal r1, r2, r3, r4
    // writeARM(0xE0E21493); // smlal r1, r2, r3, r4
    // writeARM(0xE0921493); // umulls r1, r2, r3, r4
    // writeARM(0xE0D21493); // smulls r1, r2, r3, r4
    // writeARM(0xE0B21493); // umlals r1, r2, r3, r4
    // writeARM(0xE0F21493); // smlals r1, r2, r3, r4
    // writeARM(0xEAFFFFFE); // b $

    // SMUL<x><y>, SMLA<x><y>
    // writeARM(0xE1610382); // smulbb r1, r2, r3
    // writeARM(0xE10143E2); // smlatt r1, r2, r3, r4
    // writeARM(0xEAFFFFFE); // b $

    // SMULW<y>, SMLAW<y>
    // writeARM(0xE12103A2); // smulwb r1, r2, r3
    // writeARM(0xE12143C2); // smlawt r1, r2, r3, r4
    // writeARM(0xEAFFFFFE); // b $

    // SMLAL<x><y>
    // writeARM(0xE14214C3); // smlalbt r1, r2, r3, r4
    // writeARM(0xEAFFFFFE); // b $

    // MRS
    // writeARM(0xE10F1000); // mrs r1, cpsr
    // writeARM(0xE14F1000); // mrs r1, spsr
    // writeARM(0xEAFFFFFE); // b $

    // MSR
    // writeARM(0xE12FF002); // msr cpsr_fxsc, r2
    // writeARM(0xE126F001); // msr cpsr_xs, r1
    // writeARM(0xE368F4A5); // msr spsr_f, 0xA5
    // writeARM(0xE361F01F); // msr spsr_c, 0x1F
    // writeARM(0xEAFFFFFE); // b $

    // LDR, STR, LDRB, STRB
    // writeARM(0xE5921000); // ldr r1, [r2]
    // writeARM(0xE7921003); // ldr r1, [r2, r3]
    // writeARM(0xE7821283); // str r1, [r2, r3, lsl #5]
    // writeARM(0xE5A21004); // str r1, [r2, #4]!
    // writeARM(0xE7721003); // ldrb r1, [r2, -r3]!
    // writeARM(0xE7E21323); // strb r1, [r2, r3, lsr #6]!
    // writeARM(0xE4521004); // ldrb r1, [r2], #-4
    // writeARM(0xE6C21003); // strb r1, [r2], r3
    // writeARM(0xE69212C3); // ldr r1, [r2], r3, asr #5
    // writeARM(0xE4B21003); // ldrt r1, [r2], #3
    // writeARM(0xE6A21003); // strt r1, [r2], r3
    // writeARM(0xE6F212E3); // ldrbt r1, [r2], r3, ror #5
    // writeARM(0xE59F1004); // ldr r1, [r15, #4]
    // writeARM(0xE5BF1000); // ldr r1, [r15]!
    // writeARM(0xE4BF1000); // ldrt r1, [r15]
    // writeARM(0xE5B1F000); // ldr r15, [r1]!
    // writeARM(0xE4B1F000); // ldrt r15, [r1]
    // writeARM(0xE5A1F000); // str r15, [r1]!
    // writeARM(0xE4A1F000); // strt r15, [r1]
    // writeARM(0xEAFFFFFE); // b $

    // LDRH, STRH, LDRSB, LDRSH, LDRD, STRD
    // writeARM(0xE1D010B0); // ldrh r1, [r0]
    // writeARM(0xE1C010BA); // strh r1, [r0, #10]
    // writeARM(0xE1D020D1); // ldrsb r2, [r0, #1]
    // writeARM(0xE1D030F2); // ldrsh r3, [r0, #2]
    // writeARM(0xE1C040D0); // ldrd r4, r5, [r0]
    // writeARM(0xE1C041F0); // strd r4, r5, [r0, #16]
    // writeARM(0xE1D060B2); // ldrh r6, [r0, #2]
    // writeARM(0xE19070B5); // ldrh r7, [r0, r5]
    // writeARM(0xE1F080B2); // ldrh r8, [r0, #2]!
    // writeARM(0xE1B090B5); // ldrh r9, [r0, r5]!
    // writeARM(0xE0D0A0B2); // ldrh r10, [r0], #2
    // writeARM(0xE090B0B5); // ldrh r11, [r0], r5
    // writeARM(0xE19F10B3); // ldrh r1, [r15, r3]
    // writeARM(0xE19210BF); // ldrh r1, [r2, r15]
    // writeARM(0xE192F0B3); // ldrh r15, [r2, r3]
    // writeARM(0xE1C0E0F0); // strd r14, r15, [r0]
    // writeARM(0xE1C0E0D0); // ldrd r14, r15, [r0]
    // writeARM(0xEAFFFFFE); // b $

    // PLD
    // writeARM(0xF5D3F000); // pld [r3]
    // writeARM(0xEAFFFFFE); // b $

    // SWP, SWPB
    // writeARM(0xE1002091); // swp r2, r1, [r0]
    // writeARM(0xE1402091); // swpb r2, r1, [r0]
    // writeARM(0xE103109F); // swp r1, r15, [r3]
    // writeARM(0xE10F1092); // swp r1, r2, [r15]
    // writeARM(0xE103F092); // swp r15, r2, [r3]
    // writeARM(0xEAFFFFFE); // b $

    // LDM, STM
    // writeARM(0xE8A00006); // stmia r0!, {r1-r2}
    // writeARM(0xE8800018); // stmia r0, {r3-r4}
    // writeARM(0xE9300060); // ldmdb r0!, {r5-r6}
    // writeARM(0xE9100180); // ldmdb r0, {r7-r8}
    // writeARM(0xE9A00006); // stmib r0!, {r1-r2}
    // writeARM(0xE9800018); // stmib r0, {r3-r4}
    // writeARM(0xE8300600); // ldmda r0!, {r9-r10}
    // writeARM(0xE8101800); // ldmda r0, {r11-r12}
    // writeARM(0xE8FD4000); // ldmia r13!, {r14}^
    // writeARM(0xE8ED4000); // stmia r13!, {r14}^
    // writeARM(0xE8A00000); // stmia r0!, {}
    // writeARM(0xE8AF0001); // stmia r15!, {r0}
    // writeARM(0xE8BF0000); // ldmia r15!, {}
    // writeARM(0xE9BF0000); // ldmib r15!, {}
    // writeARM(0xEAFFFFFE); // b $

    // SWI, BKPT, UDF
    // writeARM(0xEF123456); // swi #0x123456
    // writeARM(0xE1200070); // bkpt
    // writeARM(0xF0000000); // udf
    // writeARM(0xEAFFFFFE); // b $

    // MRC, MCR, MRC2, MCR2
    // writeARM(0xEE110F10); // mrc p15, 0, r0, c1, c0, 0
    // writeARM(0xEE010F10); // mcr p15, 0, r0, c1, c0, 0
    // writeARM(0xEE110E10); // mrc p14, 0, r0, c1, c0, 0
    // writeARM(0xEE010E10); // mcr p14, 0, r0, c1, c0, 0
    // writeARM(0xEE5431D5); // mrc p1, 2, r3, c4, c5, 6
    // writeARM(0xEE4431D5); // mcr p1, 2, r3, c4, c5, 6
    // writeARM(0xFE110F10); // mrc2 p15, 0, r0, c1, c0, 0
    // writeARM(0xFE010F10); // mcr2 p15, 0, r0, c1, c0, 0
    // writeARM(0xEAFFFFFE); // b $

    // Simple (useless) demo
    // writeARM(0xE3A004DE); // mov r0, #0xDE000000
    // writeARM(0xE3B004DE); // movs r0, #0xDE000000
    // writeARM(0xE38008AD); // orr r0, #0xAD0000
    // writeARM(0xE3800CBE); // orr r0, #0xBE00
    // writeARM(0xE38000EF); // orr r0, #0xEF
    // writeARM(0xE3A01A01); // mov r1, #0x1000
    // writeARM(0xE5A10004); // str r0, [r1, #4]!
    // writeARM(0xE2200475); // eor r0, #0x75000000
    // writeARM(0xE2200CA3); // eor r0, #0xA300
    // writeARM(0xE2200005); // eor r0, #0x05
    // writeARM(0xE5A10004); // str r0, [r1, #4]!
    // writeARM(0xEAFFFFFE); // b $

    // Code excerpt from real software
    // writeARM(0xE1A00620); // mov r0, r0, lsr #0xC
    // writeARM(0xE1A00600); // mov r0, r0, lsl #0xC
    // writeARM(0xE2800C40); // add r0, r0, #0x4000
    // writeARM(0xE28FE000); // add lr, pc, #0x0
    // writeARM(0xE510F004); // ldr pc, [r0, #-0x4]

    // Add with carry test
    // - Requires constant propagation and dead store elimination to fully optimize
    // writeARM(0xE3E00000); // mvn r0, #0
    // writeARM(0xE3A01001); // mov r1, #1
    // writeARM(0xE0902001); // adds r2, r0, r1
    // writeARM(0xE0A23001); // adc r3, r2, r1
    // writeARM(0xEAFFFFFE); // b $

    // User mode transfer
    // writeARM(0xE8384210); // ldmda r8!, {r4, r9, r14}
    // writeARM(0xE8F84210); // ldmia r8!, {r4, r9, r14}^
    // writeARM(0xEAFFFFFE); // b $

    armajitto::Context context{armajitto::CPUArch::ARMv5TE, sys};
    armajitto::memory::Allocator alloc{};
    auto block = alloc.Allocate<armajitto::ir::BasicBlock>(
        alloc, armajitto::ir::LocationRef{0x0100, armajitto::arm::Mode::User, thumb});

    // Translate code from memory
    armajitto::ir::Translator::Parameters params{
        .maxBlockSize = 32,
    };
    armajitto::ir::Translator translator{context, params};
    translator.Translate(*block);

    // Emit IR code manually
    // armajitto::ir::Emitter emitter{*block};

    /*auto v0 = emitter.GetRegister(armajitto::arm::GPR::R0); // ld $v0, r0
    auto v1 = emitter.LogicalShiftRight(v0, 0xc, false);    // lsr $v1, $v0, #0xc
    auto v2 = emitter.CopyVar(v1);                          // copy $v2, $v1
    auto v3 = emitter.CopyVar(v2);                          // copy $v3, $v2
    emitter.CopyVar(v3);                                    // copy $v4, $v3
    emitter.SetRegister(armajitto::arm::GPR::R0, v1);       // st r0, $v1*/

    /*auto v0 = emitter.GetRegister(armajitto::arm::GPR::R0); // mov $v0, r0  (r0 is an unknown value)
    auto v1 = emitter.BitwiseAnd(v0, 0x0000FFFF, false);    // and $v1, $v0, #0x0000ffff
    auto v2 = emitter.BitwiseOr(v1, 0x21520000, false);     // orr $v2, $v1, #0x21520000
    auto v3 = emitter.BitClear(v2, 0x0000FFFF, false);      // bic $v3, $v2, #0x0000ffff
    auto v4 = emitter.BitwiseXor(v3, 0x00004110, false);    // xor $v4, $v3, #0x00004110
    auto v5 = emitter.Move(v4, false);                      // mov $v5, $v4
    auto v6 = emitter.MoveNegated(v5, false);               // mvn $v6, $v5
    emitter.SetRegister(armajitto::arm::GPR::R0, v6);*/

    printf("translated %u instructions:\n\n", block->InstructionCount());
    for (auto *op = block->Head(); op != nullptr; op = op->Next()) {
        auto str = op->ToString();
        printf("%s\n", str.c_str());
    }

    printf("--------------------------------\n");

    /*armajitto::ir::Optimize(alloc, *block, armajitto::ir::OptimizerPasses::ConstantPropagation);
    printf("after constant propagation:\n\n");
    for (auto *op = block->Head(); op != nullptr; op = op->Next()) {
        auto str = op->ToString();
        printf("%s\n", str.c_str());
    }

    printf("--------------------------------\n");

    armajitto::ir::Optimize(alloc, *block, armajitto::ir::OptimizerPasses::DeadStoreElimination);
    printf("after dead store elimination:\n\n");
    for (auto *op = block->Head(); op != nullptr; op = op->Next()) {
        auto str = op->ToString();
        printf("%s\n", str.c_str());
    }

    printf("--------------------------------\n");

    armajitto::ir::Optimize(alloc, *block, armajitto::ir::OptimizerPasses::BasicPeepholeOptimizations);
    printf("after basic peephole optimizations:\n\n");
    for (auto *op = block->Head(); op != nullptr; op = op->Next()) {
        auto str = op->ToString();
        printf("%s\n", str.c_str());
    }

    printf("--------------------------------\n");*/

    armajitto::ir::Optimize(alloc, *block);
    printf("after all optimizations:\n\n");
    for (auto *op = block->Head(); op != nullptr; op = op->Next()) {
        auto str = op->ToString();
        printf("%s\n", str.c_str());
    }
}

int main() {
    printf("armajitto %s\n\n", armajitto::version::name);

    // testBasic();
    // testCPUID();
    testTranslator();

    return EXIT_SUCCESS;
}
