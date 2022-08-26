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

    NZ = N | Z,
    NZCV = N | Z | C | V,
};

} // namespace armajitto::arm

ENABLE_BITMASK_OPERATORS(armajitto::arm::Flags);

namespace armajitto::arm {

inline std::string FlagsStr(Flags flags, Flags affectedFlags) {
    auto flg = [](bool value, bool affected, const char *letter) {
        return value ? std::string(letter) : (affected ? std::format("({})", letter) : std::string(""));
    };
    if (flags == Flags::None) {
        return "";
    }
    auto bmFlags = BitmaskEnum(flags);
    auto bmAffFlags = BitmaskEnum(affectedFlags);
    auto n = flg(bmFlags.AnyOf(Flags::N), bmAffFlags.AnyOf(Flags::N), "n");
    auto z = flg(bmFlags.AnyOf(Flags::Z), bmAffFlags.AnyOf(Flags::Z), "z");
    auto c = flg(bmFlags.AnyOf(Flags::C), bmAffFlags.AnyOf(Flags::C), "c");
    auto v = flg(bmFlags.AnyOf(Flags::V), bmAffFlags.AnyOf(Flags::V), "v");
    return std::format("{}{}{}{}", n, z, c, v);
}

inline std::string FlagsSuffixStr(Flags flags, Flags affectedFlags) {
    auto bmFlags = BitmaskEnum(flags);
    auto dot = bmFlags.Any() ? "." : "";
    auto flagsStr = FlagsStr(flags, affectedFlags);
    return std::format("{}{}", dot, flagsStr);
}

} // namespace armajitto::arm
