#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

namespace armajitto::arm {

inline std::pair<int32_t, bool> Saturate(const int64_t value) {
    constexpr int64_t min = (int64_t)std::numeric_limits<int32_t>::min();
    constexpr int64_t max = (int64_t)std::numeric_limits<int32_t>::max();
    int32_t result = std::clamp(value, min, max);
    return {result, (result != value)};
}

inline std::pair<uint32_t, std::optional<bool>> LSL(uint32_t value, uint32_t offset) {
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

} // namespace armajitto::arm
