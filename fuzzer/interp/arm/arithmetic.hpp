#pragma once

#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>

#ifndef __has_builtin
    #define __has_builtin(x) 0
#endif

namespace interp::arm {

inline bool Saturate(const int64_t value, int32_t &result) {
    constexpr int64_t min = (int64_t)std::numeric_limits<int32_t>::min();
    constexpr int64_t max = (int64_t)std::numeric_limits<int32_t>::max();
    result = std::clamp(value, min, max);
    return (result != value);
}

inline uint32_t RotateImm(uint32_t imm, uint8_t rotate, bool &carry) {
    if (rotate == 0) {
        return imm;
    }
    rotate *= 2;
    carry = (imm >> (rotate - 1)) & 1;
    return std::rotr(imm, rotate);
}

inline uint32_t RotateImm(uint32_t imm, uint8_t rotate) {
    return std::rotr(imm, rotate * 2);
}

enum ShiftOp : uint8_t { Shift_LSL, Shift_LSR, Shift_ASR, Shift_ROR };

inline uint32_t LSL(uint32_t value, uint8_t offset, bool &carry) {
    if (offset == 0) {
        return value;
    }
    if (offset >= 32) {
        carry = (offset == 32) && (value & 1);
        return 0;
    }
    carry = (value >> (32 - offset)) & 1;
    return value << offset;
}

inline uint32_t LSR(uint32_t value, uint8_t offset, bool &carry, bool imm) {
    if (offset == 0) {
        if (imm) {
            offset = 32;
        } else {
            return value;
        }
    }
    if (offset >= 32) {
        carry = (offset == 32) && (value >> 31);
        return 0;
    }
    carry = (value >> (offset - 1)) & 1;
    return value >> offset;
}

inline uint32_t ASR(uint32_t value, uint8_t offset, bool &carry, bool imm) {
    if (offset == 0) {
        if (imm) {
            offset = 32;
        } else {
            return value;
        }
    }
    if (offset >= 32) {
        carry = value >> 31;
        return (int32_t)value >> 31;
    }
    carry = (value >> (offset - 1)) & 1;
    return (int32_t)value >> offset;
}

inline uint32_t ROR(uint32_t value, uint8_t offset, bool &carry, bool imm) {
    if (imm) {
        if (offset == 0) {
            // ROR #0 is RRX #1 when used as an immediate operand
            uint32_t msb = carry << 31;
            carry = value & 1;
            return (value >> 1) | msb;
        }
    }

    if (offset == 0) {
        return value;
    }

    offset &= 0x1F;
    value = std::rotr(value, offset);
    carry = value >> 31;
    return value;
}

template <ShiftOp op>
inline uint32_t CalcImmShift(uint32_t value, uint8_t offset, bool &carry) {
    static_assert(op >= Shift_LSL && op <= Shift_ROR, "Invalid shift operation");
    if constexpr (op == Shift_LSL) {
        return LSL(value, offset, carry);
    } else if constexpr (op == Shift_LSR) {
        return LSR(value, offset, carry, true);
    } else if constexpr (op == Shift_ASR) {
        return ASR(value, offset, carry, true);
    } else if constexpr (op == Shift_ROR) {
        return ROR(value, offset, carry, true);
    }
}

inline uint32_t ADD(uint32_t augend, uint32_t addend, bool &carry, bool &overflow) {
#if __has_builtin(__builtin_add_overflow)
    uint32_t result;
    overflow = __builtin_add_overflow((int32_t)augend, (int32_t)addend, reinterpret_cast<int32_t *>(&result));
    carry = (result < augend);
#else
    uint64_t result64 = (uint64_t)augend + (uint64_t)addend;
    uint32_t result = result64;
    carry = result64 >> 32;
    overflow = (~(augend ^ addend) & (result ^ addend)) >> 31;
#endif
    return result;
}

inline uint32_t SUB(uint32_t minuend, uint32_t subtrahend, bool &carry, bool &overflow) {
#if __has_builtin(__builtin_sub_overflow)
    uint32_t result;
    carry = (minuend >= subtrahend);
    overflow = __builtin_sub_overflow((int32_t)minuend, (int32_t)subtrahend, reinterpret_cast<int32_t *>(&result));
#else
    uint32_t result = minuend - subtrahend;
    carry = (minuend >= subtrahend);
    overflow = ((minuend ^ subtrahend) & ~(result ^ subtrahend)) >> 31;
#endif
    return result;
}

inline uint32_t ADC(uint32_t augend, uint32_t addend, bool &carry, bool &overflow) {
#if __has_builtin(__builtin_add_overflow)
    uint32_t carryAddend = (carry ? 1 : 0);
    uint32_t result;
    overflow = __builtin_add_overflow((int32_t)augend, (int32_t)addend, reinterpret_cast<int32_t *>(&result));
    overflow ^= __builtin_add_overflow((int32_t)result, (int32_t)carryAddend, reinterpret_cast<int32_t *>(&result));
    carry = carryAddend ? (result <= augend) : (result < augend);
#else
    uint32_t carryAddend = (carry ? 1 : 0);
    uint64_t result64 = (uint64_t)augend + (uint64_t)addend + (uint64_t)carryAddend;
    uint32_t result = result64;
    carry = result64 >> 32;
    uint32_t sum = augend + addend;
    overflow = ((~(augend ^ addend) & (sum ^ addend)) ^ (~(sum ^ carryAddend) & (result ^ carryAddend))) >> 31;
#endif
    return result;
}

inline uint32_t SBC(uint32_t minuend, uint32_t subtrahend, bool &carry, bool &overflow) {
#if __has_builtin(__builtin_sub_overflow)
    uint32_t carrySubtrahend = (carry ? 0 : 1);
    uint32_t result1;
    uint32_t result;
    overflow = __builtin_sub_overflow((int32_t)minuend, (int32_t)subtrahend, reinterpret_cast<int32_t *>(&result1));
    overflow ^=
        __builtin_sub_overflow((int32_t)result1, (int32_t)carrySubtrahend, reinterpret_cast<int32_t *>(&result));
    carry = (minuend >= subtrahend) && (result1 >= carrySubtrahend);
#else
    uint32_t carrySubtrahend = (carry ? 0 : 1);
    uint32_t result1 = minuend - subtrahend;
    uint32_t result = result1 - carrySubtrahend;
    carry = (minuend >= subtrahend) && (result1 >= carrySubtrahend);
    overflow = (((minuend ^ subtrahend) & ~(result1 ^ subtrahend)) ^ (result1 & ~result)) >> 31;
#endif
    return result;
}

} // namespace interp::arm
