#pragma once

#include "mode.hpp"

#include <cstdint>

namespace armajitto::arm {

union PSR {
    uint32_t u32;
    struct {
        Mode mode : 5;  // 0..4   M4-M0 - Mode bits
        uint32_t t : 1; // 5      T - State Bit       (0=ARM, 1=THUMB)
        uint32_t f : 1; // 6      F - FIQ disable     (0=Enable, 1=Disable)
        uint32_t i : 1; // 7      I - IRQ disable     (0=Enable, 1=Disable)
        uint32_t : 19;  // 8..26  Reserved
        uint32_t q : 1; // 27     Q - Sticky Overflow (1=Sticky Overflow, ARMv5TE and up only)
        uint32_t v : 1; // 28     V - Overflow Flag   (0=No Overflow, 1=Overflow)
        uint32_t c : 1; // 29     C - Carry Flag      (0=Borrow/No Carry, 1=Carry/No Borrow)
        uint32_t z : 1; // 30     Z - Zero Flag       (0=Not Zero, 1=Zero)
        uint32_t n : 1; // 31     N - Sign Flag       (0=Not Signed, 1=Signed)
    };
};
static_assert(sizeof(PSR) == sizeof(uint32_t), "PSR must be an uint32");

} // namespace armajitto::arm
