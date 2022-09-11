#pragma once

#include <armajitto/armajitto.hpp>

#include <array>
#include <cstdint>

struct FuzzerSystem : public armajitto::ISystem {
    std::array<uint8_t, 256> mem;

    static constexpr auto kFuzzMem = [] {
        std::array<uint8_t, 256> mem{};
        for (size_t i = 0; i < mem.size(); i++) {
            mem[i] = i;
        }
        return mem;
    }();

    FuzzerSystem() {
        Reset();

        m_memMap.Map(armajitto::MemoryMap::Areas::All, 0, 0, 0x100000, mem.data(), mem.size());
    }

    void Reset() {
        mem = kFuzzMem;
    }

    uint8_t MemReadByte(uint32_t address) final {
        return mem[address & 0xFF];
    }

    uint16_t MemReadHalf(uint32_t address) final {
        return *reinterpret_cast<uint16_t *>(&mem[address & 0xFE]);
    }

    uint32_t MemReadWord(uint32_t address) final {
        return *reinterpret_cast<uint16_t *>(&mem[address & 0xFC]);
    }

    void MemWriteByte(uint32_t address, uint8_t value) final {
        mem[address & 0xFF] = value;
    }

    void MemWriteHalf(uint32_t address, uint16_t value) final {
        *reinterpret_cast<uint16_t *>(&mem[address & 0xFE]) = value;
    }

    void MemWriteWord(uint32_t address, uint32_t value) final {
        *reinterpret_cast<uint16_t *>(&mem[address & 0xFC]) = value;
    }
};
