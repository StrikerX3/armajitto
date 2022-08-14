#include <armajitto/armajitto.hpp>

#include <array>
#include <cstdio>

class System : public armajitto::ISystem {
public:
    uint8_t ReadByte(uint32_t address) final {
        return Read<uint8_t>(address);
    }
    uint16_t ReadHalf(uint32_t address) final {
        return Read<uint16_t>(address);
    }
    uint32_t ReadWord(uint32_t address) final {
        return Read<uint32_t>(address);
    }

    void WriteByte(uint32_t address, uint8_t value) final {
        Write(address, value);
    }
    void WriteHalf(uint32_t address, uint16_t value) final {
        Write(address, value);
    }
    void WriteWord(uint32_t address, uint32_t value) final {
        Write(address, value);
    }

    void WriteROMByte(uint32_t address, uint8_t value) {
        rom[address & 0xFFF] = value;
    }

    void WriteROMHalf(uint32_t address, uint16_t value) {
        *reinterpret_cast<uint16_t *>(&rom[address & 0xFFE]) = value;
    }

    void WriteROMWord(uint32_t address, uint32_t value) {
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

int main() {
    printf("armajitto %s\n", armajitto::version::name);

    // System implements the armajitto::ISystem interface
    System sys{};

    sys.WriteROMWord(0x000, 0xE3A00012); // mov r0, #0x12
    sys.WriteROMWord(0x004, 0xE3801C23); // orr r1, r0, #0x2300
    sys.WriteROMWord(0x008, 0xEAFFFFFC); // b #0

    // Define a specification for the recompiler
    armajitto::Specification spec{
        .system = sys,
        .cpuModel = armajitto::CPUModel::ARMv5TE,
    };

    /*
    // Make a recompiler from the specification
    armajitto::Recompiler jit{spec};

    // Get the ARM state -- registers, coprocessors, etc.
    armajitto::arm::State &armState = jit.ARMState();

    // Convenience method to start execution at the specified address and execution state
    jit.JumpTo(0x2000000, armajitto::arm::ExecState::ARM);

    // Raise the IRQ line
    jit.IRQLine() = true;
    // Interrupts are handled in Run()

    // Run for at least 32 cycles
    uint64_t cyclesExecuted = jit.Run(32);

    // Switch to FIQ mode (also switches banked registers and updates I and F flags)
    armState.SetMode(armajitto::arm::Mode::FIQ);
    */
    return EXIT_SUCCESS;
}
