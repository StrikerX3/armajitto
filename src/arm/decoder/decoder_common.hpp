#pragma once

#include "armajitto/defs/cpu_arch.hpp"

#include <cstdint>

namespace armajitto::arm::decoder {

enum class Condition : uint8_t { EQ, NE, CS, CC, MI, PL, VS, VC, HI, LS, GE, LT, GT, LE, AL, NV };
enum class ShiftType : uint8_t { LSL, LSR, ASR, ROR };

struct RegisterSpecifiedShift {
    ShiftType type;
    bool immediate;
    uint8_t srcReg;
    union {
        uint8_t imm; // when immediate == true
        uint8_t reg; // when immediate == false
    } amount;
};

struct AddressingOffset {
    bool immediate;      // *inverted* I bit
    bool positiveOffset; // U bit
    uint8_t baseReg;
    union {
        uint16_t immValue;            // when immediate == true
        RegisterSpecifiedShift shift; // when immediate == false
    };
};

} // namespace armajitto::arm::decoder
