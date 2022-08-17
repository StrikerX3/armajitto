#pragma once

#include "armajitto/util/bitmask_enum.hpp"

namespace armajitto {

enum class Flags : uint32_t {
    None = 0,

    N = (1u << 31),
    Z = (1u << 30),
    C = (1u << 29),
    V = (1u << 28),
    Q = (1u << 27),
};

} // namespace armajitto

ENABLE_BITMASK_OPERATORS(armajitto::Flags);
