#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <tuple>
#include <utility>

namespace armajitto::arm {

inline std::pair<int32_t, bool> Saturate(const int64_t value) {
    constexpr int64_t min = (int64_t)std::numeric_limits<int32_t>::min();
    constexpr int64_t max = (int64_t)std::numeric_limits<int32_t>::max();
    int32_t result = std::clamp(value, min, max);
    return {result, (result != value)};
}

inline std::pair<uint32_t, std::optional<bool>> LSL(uint32_t value, uint8_t offset) {
    if (offset == 0) {
        return {value, std::nullopt};
    }
    if (offset >= 32) {
        return {0, (offset == 32) && (value & 1)};
    }
    bool carry = (value >> (32 - offset)) & 1;
    return {value << offset, carry};
}

inline std::pair<uint32_t, std::optional<bool>> LSR(uint32_t value, uint8_t offset) {
    if (offset == 0) {
        return {value, std::nullopt};
    }
    if (offset >= 32) {
        return {0, (offset == 32) && (value >> 31)};
    }
    bool carry = (value >> (offset - 1)) & 1;
    return {value >> offset, carry};
}

inline std::pair<uint32_t, std::optional<bool>> ASR(uint32_t value, uint8_t offset) {
    if (offset == 0) {
        return {value, std::nullopt};
    }
    if (offset >= 32) {
        bool carry = value >> 31;
        return {(int32_t)value >> 31, carry};
    }
    bool carry = (value >> (offset - 1)) & 1;
    return {(int32_t)value >> offset, carry};
}

inline std::pair<uint32_t, std::optional<bool>> ROR(uint32_t value, uint8_t offset) {
    if (offset == 0) {
        return {value, std::nullopt};
    }

    value = std::rotr(value, offset & 0x1F);
    bool carry = value >> 31;
    return {value, carry};
}

inline std::pair<uint32_t, bool> RRX(uint32_t value, bool carry) {
    uint32_t msb = carry << 31;
    carry = value & 1;
    return {(value >> 1) | msb, carry};
}

inline std::tuple<uint32_t, bool, bool> ADD(uint32_t augend, uint32_t addend) {
#if __has_builtin(__builtin_add_overflow)
    uint32_t result;
    bool overflow = __builtin_add_overflow((int32_t)augend, (int32_t)addend, reinterpret_cast<int32_t *>(&result));
    bool carry = (result < augend);
#else
    uint64_t result64 = (uint64_t)augend + (uint64_t)addend;
    uint32_t result = result64;
    bool carry = result64 >> 32;
    bool overflow = (~(augend ^ addend) & (result ^ addend)) >> 31;
#endif
    return {result, carry, overflow};
}

inline std::tuple<uint32_t, bool, bool> SUB(uint32_t minuend, uint32_t subtrahend) {
#if __has_builtin(__builtin_sub_overflow)
    uint32_t result;
    bool carry = (minuend >= subtrahend);
    bool overflow = __builtin_sub_overflow((int32_t)minuend, (int32_t)subtrahend, reinterpret_cast<int32_t *>(&result));
#else
    uint32_t result = minuend - subtrahend;
    bool carry = (minuend >= subtrahend);
    bool overflow = ((minuend ^ subtrahend) & ~(result ^ subtrahend)) >> 31;
#endif
    return {result, carry, overflow};
}

inline std::tuple<uint32_t, bool, bool> ADC(uint32_t augend, uint32_t addend, bool carry) {
#if __has_builtin(__builtin_add_overflow)
    uint32_t carryAddend = (carry ? 1 : 0);
    uint32_t result;
    bool overflow = __builtin_add_overflow((int32_t)augend, (int32_t)addend, reinterpret_cast<int32_t *>(&result));
    overflow ^= __builtin_add_overflow((int32_t)result, (int32_t)carryAddend, reinterpret_cast<int32_t *>(&result));
    carry = carryAddend ? (result <= augend) : (result < augend);
#else
    uint32_t carryAddend = (carry ? 1 : 0);
    uint64_t result64 = (uint64_t)augend + (uint64_t)addend + (uint64_t)carryAddend;
    uint32_t result = result64;
    carry = result64 >> 32;
    uint32_t sum = augend + addend;
    bool overflow = ((~(augend ^ addend) & (sum ^ addend)) ^ (~(sum ^ carryAddend) & (result ^ carryAddend))) >> 31;
#endif
    return {result, carry, overflow};
}

inline std::tuple<uint32_t, bool, bool> SBC(uint32_t minuend, uint32_t subtrahend, bool carry) {
#if __has_builtin(__builtin_sub_overflow)
    uint32_t carrySubtrahend = (carry ? 0 : 1);
    uint32_t result1;
    uint32_t result;
    bool overflow =
        __builtin_sub_overflow((int32_t)minuend, (int32_t)subtrahend, reinterpret_cast<int32_t *>(&result1));
    overflow ^=
        __builtin_sub_overflow((int32_t)result1, (int32_t)carrySubtrahend, reinterpret_cast<int32_t *>(&result));
    carry = (minuend >= subtrahend) && (result1 >= carrySubtrahend);
#else
    uint32_t carrySubtrahend = (carry ? 0 : 1);
    uint32_t result1 = minuend - subtrahend;
    uint32_t result = result1 - carrySubtrahend;
    carry = (minuend >= subtrahend) && (result1 >= carrySubtrahend);
    bool overflow = (((minuend ^ subtrahend) & ~(result1 ^ subtrahend)) ^ (result1 & ~result)) >> 31;
#endif
    return {result, carry, overflow};
}

} // namespace armajitto::arm
