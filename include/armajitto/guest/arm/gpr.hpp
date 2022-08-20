#pragma once

#include <cstdint>
#include <string>

namespace armajitto::arm {

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

inline std::string ToString(GPR gpr) {
    static constexpr const char *names[] = {"r0", "r1", "r2",  "r3",  "r4",  "r5", "r6", "r7",
                                            "r8", "r9", "r10", "r11", "r12", "sp", "lr", "pc"};
    return names[static_cast<size_t>(gpr)];
}

} // namespace armajitto::arm
