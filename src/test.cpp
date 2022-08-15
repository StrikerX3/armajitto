#include <armajitto/arm/decoder.hpp>
#include <armajitto/armajitto.hpp>
#include <armajitto/core/ir/emitter.hpp>

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

void testDecoder() {
    System sys{};

    sys.ROMWriteWord(0x0100, 0xE3A00012); // mov r0, #0x12
    sys.ROMWriteWord(0x0104, 0xE3801B0D); // orr r1, r0, #0x3400
    sys.ROMWriteWord(0x0108, 0xEAFFFFFC); // b #0

    struct CodeAccessor {
        CodeAccessor(System &sys)
            : sys{sys} {}

        uint16_t CodeReadHalf(uint32_t address) {
            return sys.MemReadHalf(address);
        }
        uint32_t CodeReadWord(uint32_t address) {
            return sys.MemReadWord(address);
        }

        System &sys;
    } mem{sys};

    armajitto::ir::Emitter emitter{};
    for (uint32_t addr = 0x100; addr <= 0x108; addr += 4) {
        auto action = armajitto::DecodeARM(emitter, mem, armajitto::CPUArch::ARMv5TE, addr);
        switch (action) {
        case armajitto::DecoderAction::Continue: printf("cont\n"); break;
        case armajitto::DecoderAction::Split: printf("split\n"); break;
        case armajitto::DecoderAction::End: printf("end\n"); break;
        case armajitto::DecoderAction::UnmappedInstruction: printf("unmapped\n"); break;
        case armajitto::DecoderAction::Unimplemented: printf("unimpl\n"); break;
        }
    }
    emitter.Process(armajitto::arm::instrs::SoftwareBreakpoint{.cond = armajitto::arm::Condition::AL});
    for (auto *op : emitter.GetBlock()) {
        auto str = op->ToString();
        printf("%s\n", str.c_str());
    }
}

int main() {
    printf("armajitto %s\n", armajitto::version::name);

    // testBasic();
    testDecoder();

    return EXIT_SUCCESS;
}
