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

    R0_usr,
    R1_usr,
    R2_usr,
    R3_usr,
    R4_usr,
    R5_usr,
    R6_usr,
    R7_usr,
    R8_usr,
    R9_usr,
    R10_usr,
    R11_usr,
    R12_usr,
    R13_usr,
    R14_usr,
    R15_usr,

    SP = R13,
    LR = R14,
    PC = R15,

    SP_usr = R13_usr,
    LR_usr = R14_usr,
    PC_usr = R15_usr,
};

} // namespace armajitto
