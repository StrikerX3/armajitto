#include <armajitto/armajitto.hpp>
#include <armajitto/host/x86_64/cpuid.hpp>
#include <armajitto/host/x86_64/x86_64_host.hpp>
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
    // armState.GPR(armajitto::arm::GPR::PC) = 0x0100 + (thumb ? 4 : 8);
    // armState.CPSR().t = thumb;

    printf("PC = %08X  T = %u\n", armState.GPR(armajitto::arm::GPR::PC), armState.CPSR().t);

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
    using CPUID = armajitto::x86_64::CPUID;
    using Vendor = armajitto::x86_64::CPUID::Vendor;
    switch (CPUID::GetVendor()) {
    case Vendor::Intel: printf("Intel CPU\n"); break;
    case Vendor::AMD: printf("AMD CPU\n"); break;
    default: printf("Unknown x86-64 CPU\n"); break;
    }
    if (CPUID::HasBMI2()) {
        printf("BMI2 available\n");
    }
    if (CPUID::HasLZCNT()) {
        printf("LZCNT available\n");
    }
    if (CPUID::HasFastPDEPAndPEXT()) {
        printf("Fast PDEP/PEXT available\n");
    }
}

void testTranslatorAndOptimizer() {
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
    // writeARM(0xE3A02012); // mov r2, #0x12
    // writeARM(0xE3A03B0D); // mov r3, #0x3400
    // writeARM(0xE3A04004); // mov r4, #0x4
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
        alloc, armajitto::ir::LocationRef{0x0108, armajitto::arm::Mode::User, thumb});

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

    /*auto val = emitter.GetRegister(armajitto::arm::GPR::R0); // ld $v0, r0  (r0 is an unknown value)
    val = emitter.BitwiseAnd(val, 0x0000FFFF, false);        // and $v1, $v0, #0x0000ffff
    val = emitter.BitwiseOr(val, 0x21520000, false);         // orr $v2, $v1, #0x21520000
    val = emitter.BitClear(val, 0x0000FFFF, false);          // bic $v3, $v2, #0x0000ffff
    val = emitter.BitwiseXor(val, 0x00004110, false);        // eor $v4, $v3, #0x00004110
    val = emitter.Move(val, false);                          // mov $v5, $v4
    val = emitter.MoveNegated(val, false);                   // mvn $v6, $v5
    emitter.SetRegister(armajitto::arm::GPR::R0, val);       // st r0, $v6*/

    /*auto val = emitter.GetRegister(armajitto::arm::GPR::R0); // ld $v0, r0  (r0 is an unknown value)
    val = emitter.BitwiseAnd(val, 0x0000FFFF, false);        // and $v1, $v0, #0x0000ffff
    val = emitter.BitwiseOr(val, 0xD15D0000, false);         // orr $v2, $v1, #0xd15d0000
    val = emitter.BitwiseXor(val, 0xF00F4110, false);        // eor $v3, $v2, #0xf00f4110
    val = emitter.MoveNegated(val, false);                   // mvn $v4, $v3
    emitter.SetRegister(armajitto::arm::GPR::R0, val);       // st r0, $v4*/

    /*auto val = emitter.GetRegister(armajitto::arm::GPR::R0); // ld $v0, r0  (r0 is an unknown value)
    val = emitter.BitwiseAnd(val, 0x0000FFFF, false);        // and $v1, $v0, #0x0000ffff
    val = emitter.BitwiseOr(val, 0x21520000, false);         // orr $v2, $v1, #0x21520000
    val = emitter.BitwiseXor(val, 0xF00FF00F, false);        // eor $v3, $v2, #0xf00ff00f
    val = emitter.MoveNegated(val, false);                   // mvn $v4, $v3
    emitter.SetRegister(armajitto::arm::GPR::R0, val);       // st r0, $v4*/

    // Bitwise ops coalescence test
    /*constexpr auto flgC = armajitto::arm::Flags::C;
    constexpr auto flgNone = armajitto::arm::Flags::None;
    auto val = emitter.GetRegister(armajitto::arm::GPR::R0); // ld $v0, r0  (r0 is an unknown value)
    val = emitter.BitwiseAnd(val, 0x0000FFFF, false);        // and $v1, $v0, #0x0000ffff
    val = emitter.BitwiseOr(val, 0xFFFF0000, false);         // orr $v2, $v1, #0xffff0000
    val = emitter.BitwiseXor(val, 0xF00F0FF0, false);        // eor $v3, $v2, #0xf00f0ff0
    val = emitter.MoveNegated(val, false);                   // mvn $v4, $v3
    val = emitter.ArithmeticShiftRight(val, 12, false);      // asr $v5, $v4, #0xc
    val = emitter.LogicalShiftLeft(val, 8, false);           // lsl $v6, $v5, #0x8
    val = emitter.LogicalShiftRight(val, 4, false);          // lsr $v7, $v6, #0x4
    val = emitter.RotateRight(val, 3, false);                // ror $v8, $v7, #0x3
    emitter.StoreFlags(flgC, flgC);                          // stflg.c {c}
    val = emitter.RotateRightExtended(val, false);           // rrx $v9, $v8
    emitter.SetRegister(armajitto::arm::GPR::R0, val);       // st r0, $v8*/

    // Host flag ops coalescence test
    /*constexpr auto flgNone = armajitto::arm::Flags::None;
    constexpr auto flgZ = armajitto::arm::Flags::Z;
    constexpr auto flgC = armajitto::arm::Flags::C;
    constexpr auto flgV = armajitto::arm::Flags::V;
    constexpr auto flgQ = armajitto::arm::Flags::Q;
    constexpr auto flgNZ = armajitto::arm::Flags::NZ;
    constexpr auto flgCV = flgC | flgV;
    constexpr auto flgNZC = flgNZ | flgC;
    emitter.StoreFlags(flgNZ, flgNone);                     // stflg.nz {}
    emitter.StoreFlags(flgC, flgNone);                      // stflg.c {}
    emitter.StoreFlags(flgV, flgNone);                      // stflg.v {}
    emitter.StoreFlags(flgQ, flgQ);                         // stflg.q {q}
    auto v0 = emitter.GetRegister(armajitto::arm::GPR::R0); // ld $v0, r0
    auto v1 = emitter.GetRegister(armajitto::arm::GPR::R1); // ld $v1, r1
    emitter.StoreFlags(flgC, flgNone);                      // stflg.c {}
    emitter.AddCarry(v1, v0, true);                         // adc.nzcv $v1, $v0
    emitter.StoreFlags(flgQ, flgQ);                         // stflg.q {q}
    emitter.StoreFlags(flgCV, flgNone);                     // stflg.cv {}
    emitter.StoreFlags(flgNZ, flgNZ);                       // stflg.nz {nz}
    emitter.LoadFlags(flgNZC);                              // ld $v2, cpsr; ldflg.nzc $v3, $v2; st cpsr, $v3
    emitter.StoreFlags(flgZ, flgZ);                         // stflg.z {z}
    emitter.StoreFlags(flgV, flgV);                         // stflg.v {v}
    emitter.StoreFlags(flgCV, flgNone);                     // stflg.cv {}*/

    /*auto cpsr = emitter.GetCPSR();                          // ld $v0, cpsr
    auto mod = emitter.Add(cpsr, 4, false);                 // add $v1, $v0, #0x4
    emitter.SetRegister(armajitto::arm::GPR::R0, mod);      // st r0_usr, $v1
    emitter.SetCPSR(cpsr);                                  // st cpsr, $v0
    auto cpsr2 = emitter.GetCPSR();                         // ld $v2, cpsr
    cpsr2 = emitter.BitClear(cpsr2, 0xc0000000, false);     // bic $v3, $v2, #0xc0000000
    emitter.SetCPSR(cpsr2);                                 // st cpsr, $v3
    auto r5 = emitter.GetRegister(armajitto::arm::GPR::R5); // ld $v4, r5
    emitter.SetCPSR(r5);                                    // st cpsr, $v4*/

    // Identity operation elimination test
    /*auto var = emitter.GetRegister(armajitto::arm::GPR::R0);
    var = emitter.LogicalShiftLeft(var, 0, false);     // lsl <var>, <var>, 0
    var = emitter.LogicalShiftRight(var, 0, false);    // lsr <var>, <var>, 0
    var = emitter.ArithmeticShiftRight(var, 0, false); // asr <var>, <var>, 0
    var = emitter.RotateRight(var, 0, false);          // ror <var>, <var>, 0
    var = emitter.BitwiseAnd(var, ~0, false);          // and <var>, <var>, 0xFFFFFFFF
    var = emitter.BitwiseAnd(~0, var, false);          // and <var>, 0xFFFFFFFF, <var>
    var = emitter.BitwiseOr(var, 0, false);            // orr <var>, <var>, 0
    var = emitter.BitwiseOr(0, var, false);            // orr <var>, 0, <var>
    var = emitter.BitwiseXor(var, 0, false);           // eor <var>, <var>, 0
    var = emitter.BitwiseXor(0, var, false);           // eor <var>, 0, <var>
    var = emitter.BitClear(var, 0, false);             // bic <var>, <var>, 0
    var = emitter.BitClear(0, var, false);             // bic <var>, 0, <var>
    var = emitter.Add(var, 0, false);                  // add <var>, <var>, 0
    var = emitter.Add(0, var, false);                  // add <var>, 0, <var>
    var = emitter.Subtract(0, var, false);             // sub <var>, <var>, 0
    emitter.StoreFlags(armajitto::arm::Flags::C, armajitto::arm::Flags::None);
    var = emitter.AddCarry(var, 0, false); // adc <var>, <var>, 0 (with known ~C)
    var = emitter.AddCarry(0, var, false); // adc <var>, 0, <var> (with known ~C)
    emitter.StoreFlags(armajitto::arm::Flags::C, armajitto::arm::Flags::C);
    var = emitter.SubtractCarry(var, 0, false);                     // sbc  <var>, <var>, 0 (with known  C)
    var = emitter.SaturatingAdd(var, 0, false);                     // qadd <var>, <var>, 0
    var = emitter.SaturatingAdd(0, var, false);                     // qadd <var>, 0, <var>
    var = emitter.SaturatingSubtract(var, 0, false);                // qsub <var>, <var>, 0
    var = emitter.Multiply(var, 1, false, false);                   // umul <var>, <var>, 1
    var = emitter.Multiply(1, var, false, false);                   // umul <var>, 1, <var>
    var = emitter.Multiply(var, 1, true, false);                    // smul <var>, <var>, 1
    var = emitter.Multiply(1, var, true, false);                    // smul <var>, 1, <var>
    auto dual1 = emitter.MultiplyLong(var, 1, false, false, false); // umull <var>:<var>, <var>, 1
    auto dual2 = emitter.MultiplyLong(1, var, false, false, false); // umull <var>:<var>, 1, <var>
    auto dual3 = emitter.MultiplyLong(var, 1, true, false, false);  // smull <var>:<var>, <var>, 1
    auto dual4 = emitter.MultiplyLong(1, var, true, false, false);  // smull <var>:<var>, 1, <var>
    auto dual5 = emitter.AddLong(var, var, 0, 0, false);            // addl <var>:<var>, <var>:<var>, 0:0
    auto dual6 = emitter.AddLong(0, 0, var, var, false);            // addl <var>:<var>, 0:0, <var>:<var>
    emitter.SetRegister(armajitto::arm::GPR::R1, dual1.lo);
    emitter.SetRegister(armajitto::arm::GPR::R2, dual1.hi);
    emitter.SetRegister(armajitto::arm::GPR::R3, dual2.lo);
    emitter.SetRegister(armajitto::arm::GPR::R4, dual2.hi);
    emitter.SetRegister(armajitto::arm::GPR::R5, dual3.lo);
    emitter.SetRegister(armajitto::arm::GPR::R6, dual3.hi);
    emitter.SetRegister(armajitto::arm::GPR::R7, dual4.lo);
    emitter.SetRegister(armajitto::arm::GPR::R8, dual4.hi);
    emitter.SetRegister(armajitto::arm::GPR::R9, dual5.lo);
    emitter.SetRegister(armajitto::arm::GPR::R10, dual5.hi);
    emitter.SetRegister(armajitto::arm::GPR::R11, dual6.lo);
    emitter.SetRegister(armajitto::arm::GPR::R12, dual6.hi);*/

    // Arithmetic ops coalescence test
    /*constexpr auto flgNone = armajitto::arm::Flags::None;
    constexpr auto flgC = armajitto::arm::Flags::C;
    auto val = emitter.GetRegister(armajitto::arm::GPR::R0); // ld $v0, r0
    val = emitter.Add(val, 3, false);                        // add $v1, $v0, 3        $v0 + 3
    val = emitter.Add(val, 6, false);                        // add $v2, $v1, 6        $v0 + 9
    val = emitter.Subtract(val, 5, false);                   // sub $v3, $v2, 5        $v0 + 4
    val = emitter.Subtract(val, 4, false);                   // sub $v4, $v3, 4        $v0
    val = emitter.Add(2, val, false);                        // add $v5, 2, $v4        $v0 + 2
    val = emitter.Subtract(5, val, false);                   // sub $v6, 5, $v5        3 - $v0
    emitter.StoreFlags(flgC, flgC);                          // stflg.c {c}         C
    val = emitter.AddCarry(val, 0, false);                   // adc $v7, $v6, 0     C  4 - $v0
    emitter.StoreFlags(flgC, flgNone);                       // stflg.c {}
    val = emitter.AddCarry(val, 1, false);                   // adc $v8, $v7, 1        5 - $v0
    emitter.StoreFlags(flgC, flgC);                          // stflg.c {c}         C
    val = emitter.AddCarry(1, val, false);                   // adc $v9, 1, $v8     C  7 - $v0
    emitter.StoreFlags(flgC, flgNone);                       // stflg.c {}
    val = emitter.AddCarry(2, val, false);                   // adc $v10, 2, $v9       9 - $v0
    emitter.StoreFlags(flgC, flgC);                          // stflg.c {c}         C
    val = emitter.SubtractCarry(val, 1, false);              // sbc $v11, $v10, 1   C  8 - $v0
    emitter.StoreFlags(flgC, flgNone);                       // stflg.c {}
    val = emitter.SubtractCarry(val, 0, false);              // sbc $v12, $v11, 0      7 - $v0
    emitter.StoreFlags(flgC, flgC);                          // stflg.c {c}         C
    val = emitter.SubtractCarry(2, val, false);              // sbc $v13, 2, $v12   C  $v0 - 5
    emitter.StoreFlags(flgC, flgNone);                       // stflg.c {}
    val = emitter.SubtractCarry(1, val, false);              // sbc $v14, 1, $v13      5 - $v0
    val = emitter.CopyVar(val);                              // copy $v15, $v14        5 - $v0
    val = emitter.Move(val, false);                          // mov $v16, $v15         5 - $v0
    val = emitter.MoveNegated(val, false);                   // mvn $v17, $v16         $v0 - 6
    val = emitter.BitwiseXor(val, ~0, false);                // eor $v18, $v17, ~0     5 - $v0
    emitter.SetRegister(armajitto::arm::GPR::R0, val);       // st r0, $v18*/

    // GPR and PSR optimization edge cases
    // These should be completely erased
    /*auto cpsr = emitter.GetCPSR();                          // ld $v0, cpsr
    emitter.SetCPSR(cpsr);                                  // st cpsr, $v0
    auto r0 = emitter.GetRegister(armajitto::arm::GPR::R0); // ld $v1, r0
    emitter.SetRegister(armajitto::arm::GPR::R0, r0);       // st r0, $v1*/

    // Multiply long with half shift
    /*// auto r0 = emitter.GetRegister(armajitto::arm::GPR::R0);           // ld $v0, r0
    auto r0 = emitter.Constant(0x10000);                    // const $v0, #0x10000
    auto r1 = emitter.GetRegister(armajitto::arm::GPR::R1); // ld $v1, r1
    // auto r1 = emitter.Constant(0x1234);                               // const $v1, #0x1234
    auto [lo, hi] = emitter.MultiplyLong(r0, r1, true, true, false); // smullh $v3:$v2, $v0, $v1
    emitter.SetRegister(armajitto::arm::GPR::R2, lo);                // st r2, $v2
    emitter.SetRegister(armajitto::arm::GPR::R3, hi);                // st r3, $v3*/

    auto printBlock = [&] {
        for (auto *op = block->Head(); op != nullptr; op = op->Next()) {
            auto str = op->ToString();
            printf("%s\n", str.c_str());
        }
    };

    printf("translated %u instructions:\n\n", block->InstructionCount());
    printBlock();

    using OptPass = armajitto::ir::OptimizerPasses;

    bool printSep = false;

    auto runOptimizer = [&](OptPass pass, const char *name) {
        if (printSep) {
            printSep = false;
            printf("--------------------------------\n");
        }
        if (armajitto::ir::Optimize(alloc, *block, pass, false)) {
            printf("after %s:\n\n", name);
            printBlock();
            printSep = true;
            return true;
        } else {
            printf("%s made no changes\n", name);
            return false;
        }
    };

    bool optimized;
    int i = 0;
    do {
        printSep = false;
        optimized = false;
        ++i;

        printf("\n==================================================\n");
        printf("  iteration %d\n", i);
        printf("==================================================\n\n");

        optimized |= runOptimizer(OptPass::ConstantPropagation, "constant propagation");
        optimized |= runOptimizer(OptPass::DeadRegisterStoreElimination, "dead register store elimination");
        optimized |= runOptimizer(OptPass::DeadGPRStoreElimination, "dead GPR store elimination");
        optimized |= runOptimizer(OptPass::DeadHostFlagStoreElimination, "dead host flag store elimination");
        optimized |= runOptimizer(OptPass::DeadFlagValueStoreElimination, "dead flag value store elimination");
        optimized |= runOptimizer(OptPass::DeadVarStoreElimination, "dead variable store elimination");
        optimized |= runOptimizer(OptPass::BitwiseOpsCoalescence, "bitwise operations coalescence");
        optimized |= runOptimizer(OptPass::ArithmeticOpsCoalescence, "arithmetic operations coalescence");
        optimized |= runOptimizer(OptPass::HostFlagsOpsCoalescence, "host flags operations coalescence");
    } while (optimized);

    printf("\n==================================================\n");
    printf("  finished\n");
    printf("==================================================\n\n");

    armajitto::ir::Optimize(alloc, *block);
    printf("after all optimizations:\n\n");
    printBlock();
}

void testCompiler() {
    System sys{};

    armajitto::Context context{armajitto::CPUArch::ARMv5TE, sys};

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

    // writeARM(0xE3A02012); // mov r2, #0x12
    // writeARM(0xE3A03B0D); // mov r3, #0x3400
    // writeARM(0xE3A04004); // mov r4, #0x4
    // writeARM(0xE0121003); // ands r1, r2, r3
    // writeARM(0xE0321383); // eors r1, r2, r3, lsl #7
    // writeARM(0xE0521413); // subs r1, r2, r3, lsl r4
    // writeARM(0xE07213A3); // rsbs r1, r2, r3, lsr #7
    writeARM(0xE0921433); // adds r1, r2, r3, lsr r4
    // writeARM(0xE0B213C3); // adcs r1, r2, r3, asr #7
    // writeARM(0xE0D21453); // sbcs r1, r2, r3, asr r4
    // writeARM(0xE0F213E3); // rscs r1, r2, r3, ror #7
    // writeARM(0xE1120003); // tst r2, r3
    // writeARM(0xE1320003); // teq r2, r3
    // writeARM(0xE1520003); // cmp r2, r3
    // writeARM(0xE1720003); // cmn r2, r3
    // writeARM(0xE1921473); // orrs r1, r2, r3, ror r4
    // writeARM(0xE1B01002); // movs r1, r2
    writeARM(0xE1D21063); // bics r1, r2, r3, rrx
    // writeARM(0xE1E01003); // mvn r1, r3
    writeARM(0xEAFFFFFE); // b $

    // Create allocator
    armajitto::memory::Allocator alloc{};
    auto block = alloc.Allocate<armajitto::ir::BasicBlock>(
        alloc, armajitto::ir::LocationRef{0x0108, armajitto::arm::Mode::User, thumb});

    // Translate code from memory
    armajitto::ir::Translator::Parameters params{
        .maxBlockSize = 32,
    };
    armajitto::ir::Translator translator{context, params};
    translator.Translate(*block);

    // Optimize code
    armajitto::ir::Optimize(alloc, *block);

    // Display IR code
    printf("translated %u instructions:\n\n", block->InstructionCount());
    for (auto *op = block->Head(); op != nullptr; op = op->Next()) {
        auto str = op->ToString();
        printf("%s\n", str.c_str());
    }
    printf("\n");

    // Define function to display ARM state
    auto printState = [&] {
        auto &state = context.GetARMState();
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

        auto &cpsr = state.CPSR();
        printf("CPSR = %08X   ", cpsr.u32);
        switch (cpsr.mode) {
        case armajitto::arm::Mode::User: printf("USR"); break;
        case armajitto::arm::Mode::FIQ: printf("FIQ"); break;
        case armajitto::arm::Mode::IRQ: printf("IRQ"); break;
        case armajitto::arm::Mode::Supervisor: printf("SVC"); break;
        case armajitto::arm::Mode::Abort: printf("ABT"); break;
        case armajitto::arm::Mode::Undefined: printf("UND"); break;
        case armajitto::arm::Mode::System: printf("SYS"); break;
        default: printf("%02Xh", static_cast<uint8_t>(cpsr.mode)); break;
        }
        if (cpsr.t) {
            printf("  THUMB");
        } else {
            printf("  ARM  ");
        }
        printf("%c%c%c%c%c%c%c\n", (cpsr.n ? 'N' : '.'), (cpsr.z ? 'Z' : '.'), (cpsr.c ? 'C' : '.'),
               (cpsr.v ? 'V' : '.'), (cpsr.q ? 'Q' : '.'), (cpsr.i ? 'I' : '.'), (cpsr.f ? 'F' : '.'));
    };

    // Setup initial ARM state
    auto &armState = context.GetARMState();
    armState.JumpTo(baseAddress, thumb);
    // armState.GPR(armajitto::arm::GPR::R2) = 0x12;
    armState.GPR(armajitto::arm::GPR::R2) = -1;
    armState.GPR(armajitto::arm::GPR::R3) = 0x3400;
    armState.GPR(armajitto::arm::GPR::R4) = 4;
    armState.CPSR().n = 1;
    armState.CPSR().z = 1;
    armState.CPSR().c = 0;
    armState.CPSR().v = 0;

    printf("state before execution:\n");
    printState();

    // Compile and execute code
    armajitto::x86_64::x64Host host{context};
    printf("\ncompiling code...\n");
    host.Compile(*block);
    printf("done; invoking\n");
    host.Call(*block);
    printf("\n");

    printf("state after execution:\n");
    printState();
}

int main() {
    printf("armajitto %s\n\n", armajitto::version::name);

    // testCPUID();
    // testBasic();
    // testTranslatorAndOptimizer();
    testCompiler();

    return EXIT_SUCCESS;
}
