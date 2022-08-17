#include <armajitto/armajitto.hpp>
#include <armajitto/host/x86_64/cpuid.hpp>
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
    bool thumb;

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

    // ARM ALU operations
    // writeARM(0xE0121003); // ands r1, r2, r3
    // writeARM(0xE0321383); // eors r1, r2, r3, lsl #7
    // writeARM(0xE0521413); // subs r1, r2, r3, lsl r4
    // writeARM(0xE07213A3); // rsbs r1, r2, r3, lsr #7
    // writeARM(0xE0921433); // adds r1, r2, r3, lsr r4
    // writeARM(0xE0B213C3); // adcs r1, r2, r3, asr #7
    // writeARM(0xE0D21453); // sbcs r1, r2, r3, asr r4
    // writeARM(0xE0F213E3); // rscs r1, r2, r3, ror #7
    // writeARM(0xE1120003); // tst r2, r3
    // writeARM(0xE1320003); // teq r2, r3
    // writeARM(0xE1520003); // cmp r2, r3
    // writeARM(0xE1720003); // cmn r2, r3
    // writeARM(0xE1921473); // orrs r1, r2, r3, ror r4
    // writeARM(0xE1B01002); // movs r1, r2
    // writeARM(0xE1D21063); // bics r1, r2, r3, rrx
    // writeARM(0xE1E01003); // mvn r1, r3
    // writeARM(0xEAFFFFFE); // b $

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
    writeARM(0xE1610382); // smulbb r1, r2, r3
    writeARM(0xE10143E2); // smlatt r1, r2, r3, r4
    writeARM(0xEAFFFFFE); // b $

    // Thumb branches
    // writeThumb(0xF7FF); // bl $ (prefix)
    // writeThumb(0xFFFE); // bl $ (suffix)
    // writeThumb(0xF7FF); // blx $ (prefix)
    // writeThumb(0xEFFE); // blx $ (suffix)
    // writeThumb(0xD0FE); // beq $
    // writeThumb(0xE7FE); // b $
    // writeThumb(0x4708); // bx r1
    // writeThumb(0x4788); // blx r1

    armajitto::Context context{armajitto::CPUArch::ARMv5TE, sys};
    armajitto::ir::BasicBlock block{{0x0100, armajitto::arm::Mode::User, thumb}};

    armajitto::ir::Translator::Parameters params{
        .maxBlockSize = 32,
    };

    armajitto::ir::Translator translator{context, params};
    translator.Translate(block);
}

int main() {
    printf("armajitto %s\n", armajitto::version::name);

    // testBasic();
    testCPUID();
    testTranslator();

    return EXIT_SUCCESS;
}
