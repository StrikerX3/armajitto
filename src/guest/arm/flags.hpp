#pragma once

#include "util/bitmask_enum.hpp"

#include <cstdint>
#include <sstream>
#include <string>

namespace armajitto::arm {

enum class Flags : uint32_t {
    None = 0,

    N = (1u << 31),
    Z = (1u << 30),
    C = (1u << 29),
    V = (1u << 28),

    NZ = N | Z,
    NZC = N | Z | C,
    NZCV = N | Z | C | V,
};

} // namespace armajitto::arm

ENABLE_BITMASK_OPERATORS(armajitto::arm::Flags);

namespace armajitto::arm {

inline std::string FlagsStr(Flags flags, Flags affectedFlags) {
    auto flg = [](bool value, bool affected, const char *letter) {
        if (value) {
            return std::string(letter);
        } else if (affected) {
            return std::string("(") + letter + std::string(")");
        } else {
            return std::string("");
        }
    };
    if (flags == Flags::None) {
        return "";
    }

    auto bmFlags = BitmaskEnum(flags);
    auto bmAffFlags = BitmaskEnum(affectedFlags);

    std::ostringstream oss;
    oss << flg(bmFlags.AnyOf(Flags::N), bmAffFlags.AnyOf(Flags::N), "n");
    oss << flg(bmFlags.AnyOf(Flags::Z), bmAffFlags.AnyOf(Flags::Z), "z");
    oss << flg(bmFlags.AnyOf(Flags::C), bmAffFlags.AnyOf(Flags::C), "c");
    oss << flg(bmFlags.AnyOf(Flags::V), bmAffFlags.AnyOf(Flags::V), "v");
    return oss.str();
}

inline std::string FlagsSuffixStr(Flags flags, Flags affectedFlags) {
    auto bmFlags = BitmaskEnum(flags);
    std::string dot = bmFlags.Any() ? "." : "";
    std::string flagsStr = FlagsStr(flags, affectedFlags);
    return dot + flagsStr;
}

} // namespace armajitto::arm
