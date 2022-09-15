#pragma once

#include <armajitto/armajitto.hpp>

#include <array>
#include <cstdint>

struct FuzzerSystem : public armajitto::ISystem {
    alignas(16) std::array<uint8_t, 256> mem;
    alignas(16) std::array<uint8_t, 256> codemem;   // mapped to 0x10000..0x100FF
    alignas(16) std::array<uint8_t, 32> loexcptmem; // mapped to 0x00000000..0x0000001F
    alignas(16) std::array<uint8_t, 32> hiexcptmem; // mapped to 0xFFFF0000..0xFFFF001F

    static constexpr auto kFuzzMem = [] {
        std::array<uint8_t, 256> mem{};
        for (size_t i = 0; i < mem.size(); i++) {
            mem[i] = i;
        }
        return mem;
    }();

    static constexpr auto kLoExcptMem = [] {
        std::array<uint8_t, 32> mem{};
        size_t i = 0;
        for (uint32_t instr : {
                 0xEA003FFE, // [00000000] RST  -> b #0x10000
                 0xE1B0F00E, // [00000004] UND  -> movs pc, lr
                 0xE1B0F00E, // [00000008] SWI  -> movs pc, lr
                 0xE25EF004, // [0000000C] PABT -> subs pc, lr, #4
                 0xE25EF004, // [00000010] DABT -> subs pc, lr, #4
                 0xE25EF004, // [00000018] IRQ  -> subs pc, lr, #4
                 0xE25EF004, // [0000001C] FIQ  -> subs pc, lr, #4
             }) {
            mem[i++] = instr >> 0;
            mem[i++] = instr >> 8;
            mem[i++] = instr >> 16;
            mem[i++] = instr >> 24;
        }
        return mem;
    }();

    static constexpr auto kHiExcptMem = [] {
        std::array<uint8_t, 32> mem = kLoExcptMem;
        size_t i = 0;
        for (uint32_t instr : {
                 0xEA007FFE, // [FFFF0000] RST  -> b #0x10000
             }) {
            mem[i++] = instr >> 0;
            mem[i++] = instr >> 8;
            mem[i++] = instr >> 16;
            mem[i++] = instr >> 24;
        }
        return mem;
    }();

    FuzzerSystem() {
        Reset();
    }

    void Reset() {
        mem = kFuzzMem;
        codemem.fill(0);
        loexcptmem = kLoExcptMem;
        hiexcptmem = kHiExcptMem;
    }

    uint8_t MemReadByte(uint32_t address) final {
        if (address >= 0x00 && address <= 0x1F) {
            return loexcptmem[address & 0x1F];
        } else if (address >= 0xFFFF0000 && address <= 0xFFFF001F) {
            return hiexcptmem[address & 0x1F];
        } else if (address >= 0x10000 && address <= 0x100FF) {
            return codemem[address & 0xFF];
        } else {
            return mem[address & 0xFF];
        }
    }

    uint16_t MemReadHalf(uint32_t address) final {
        if (address >= 0x00 && address <= 0x1F) {
            return *reinterpret_cast<uint16_t *>(&loexcptmem[address & 0x1E]);
        } else if (address >= 0xFFFF0000 && address <= 0xFFFF001F) {
            return *reinterpret_cast<uint16_t *>(&hiexcptmem[address & 0x1E]);
        } else if (address >= 0x10000 && address <= 0x100FF) {
            return *reinterpret_cast<uint16_t *>(&codemem[address & 0xFE]);
        } else {
            return *reinterpret_cast<uint16_t *>(&mem[address & 0xFE]);
        }
    }

    uint32_t MemReadWord(uint32_t address) final {
        if (address >= 0x00 && address <= 0x1F) {
            return *reinterpret_cast<uint32_t *>(&loexcptmem[address & 0x1C]);
        } else if (address >= 0xFFFF0000 && address <= 0xFFFF001F) {
            return *reinterpret_cast<uint32_t *>(&hiexcptmem[address & 0x1C]);
        } else if (address >= 0x10000 && address <= 0x100FF) {
            return *reinterpret_cast<uint32_t *>(&codemem[address & 0xFC]);
        } else {
            return *reinterpret_cast<uint32_t *>(&mem[address & 0xFC]);
        }
    }

    void MemWriteByte(uint32_t address, uint8_t value) final {
        mem[address & 0xFF] = value;
    }

    void MemWriteHalf(uint32_t address, uint16_t value) final {
        *reinterpret_cast<uint16_t *>(&mem[address & 0xFE]) = value;
    }

    void MemWriteWord(uint32_t address, uint32_t value) final {
        *reinterpret_cast<uint32_t *>(&mem[address & 0xFC]) = value;
    }
};
