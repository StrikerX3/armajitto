#pragma once

#include <armajitto/armajitto.hpp>

#include <array>
#include <cstdint>

struct FuzzerSystem : public armajitto::ISystem {
    std::array<uint8_t, 256> mem;
    std::array<uint8_t, 256> codemem; // mapped to 0x10000..0x100FF

    static constexpr auto kFuzzMem = [] {
        std::array<uint8_t, 256> mem{};
        for (size_t i = 0; i < mem.size(); i++) {
            mem[i] = i;
        }
        return mem;
    }();

    FuzzerSystem() {
        Reset();
    }

    void Reset() {
        mem = kFuzzMem;
        codemem.fill(0);
    }

    uint8_t MemReadByte(uint32_t address) final {
        if (address >= 0x10000 && address <= 0x100FF) {
            return codemem[address & 0xFF];
        } else {
            return mem[address & 0xFF];
        }
    }

    uint16_t MemReadHalf(uint32_t address) final {
        if (address >= 0x10000 && address <= 0x100FF) {
            return *reinterpret_cast<uint16_t *>(&codemem[address & 0xFE]);
        } else {
            return *reinterpret_cast<uint16_t *>(&mem[address & 0xFE]);
        }
    }

    uint32_t MemReadWord(uint32_t address) final {
        if (address >= 0x10000 && address <= 0x100FF) {
            return *reinterpret_cast<uint16_t *>(&codemem[address & 0xFC]);
        } else {
            return *reinterpret_cast<uint16_t *>(&mem[address & 0xFC]);
        }
    }

    void MemWriteByte(uint32_t address, uint8_t value) final {
        if (address >= 0x10000 && address <= 0x100FF) {
            codemem[address & 0xFF] = value;
        } else {
            mem[address & 0xFF] = value;
        }
    }

    void MemWriteHalf(uint32_t address, uint16_t value) final {
        if (address >= 0x10000 && address <= 0x100FF) {
            *reinterpret_cast<uint16_t *>(&codemem[address & 0xFE]) = value;
        } else {
            *reinterpret_cast<uint16_t *>(&mem[address & 0xFE]) = value;
        }
    }

    void MemWriteWord(uint32_t address, uint32_t value) final {
        if (address >= 0x10000 && address <= 0x100FF) {
            *reinterpret_cast<uint16_t *>(&codemem[address & 0xFC]) = value;
        } else {
            *reinterpret_cast<uint16_t *>(&mem[address & 0xFC]) = value;
        }
    }
};
