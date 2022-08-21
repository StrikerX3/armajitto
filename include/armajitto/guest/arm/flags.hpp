#pragma once

#include "armajitto/util/bitmask_enum.hpp"

#include <format>
#include <string>

namespace armajitto::arm {

enum class Flags : uint32_t {
    None = 0,

    N = (1u << 31),
    Z = (1u << 30),
    C = (1u << 29),
    V = (1u << 28),
    Q = (1u << 27),
};

} // namespace armajitto::arm

ENABLE_BITMASK_OPERATORS(armajitto::arm::Flags);

namespace armajitto::arm {

// Common flag combinations
static constexpr Flags kFlagsNZ = Flags::N | Flags::Z;
static constexpr Flags kFlagsNZCV = kFlagsNZ | Flags::C | Flags::V;
static constexpr Flags kFlagsNZCVQ = kFlagsNZCV | Flags::Q;

inline std::string FlagsSuffixStr(Flags flags) {
    auto flg = [](bool value, const char *letter) { return value ? std::string(letter) : std::string(""); };
    auto bmFlags = BitmaskEnum(flags);
    auto dot = flg(bmFlags.Any(), ".");
    auto n = flg(bmFlags.AnyOf(Flags::N), "n");
    auto z = flg(bmFlags.AnyOf(Flags::Z), "z");
    auto c = flg(bmFlags.AnyOf(Flags::C), "c");
    auto v = flg(bmFlags.AnyOf(Flags::V), "v");
    auto q = flg(bmFlags.AnyOf(Flags::Q), "q");
    return std::format("{}{}{}{}{}{}", dot, n, z, c, v, q);
}

} // namespace armajitto::arm
