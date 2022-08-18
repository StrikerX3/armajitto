#pragma once

#include <cstdint>

namespace armajitto {

enum class GPR : uint8_t {
    R0,
    R1,
    R2,
    R3,
    R4,
    R5,
    R6,
    R7,
    R8,
    R9,
    R10,
    R11,
    R12,
    R13,
    R14,
    R15,

    SP = R13,
    LR = R14,
    PC = R15,
};

} // namespace armajitto
