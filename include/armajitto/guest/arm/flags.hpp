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

    NZ = N | Z,
    NZCV = N | Z | C | V,
    NZCVQ = N | Z | C | V | Q,
};

} // namespace armajitto::arm

ENABLE_BITMASK_OPERATORS(armajitto::arm::Flags);

namespace armajitto::arm {

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
