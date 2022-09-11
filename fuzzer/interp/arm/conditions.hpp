#pragma once

#include <array>
#include <cstdint>

namespace interp::arm {

// Condition flags
enum ConditionFlags : uint32_t {
    Cond_EQ, // EQ     Z=1           equal (zero) (same)
    Cond_NE, // NE     Z=0           not equal (nonzero) (not same)
    Cond_CS, // CS/HS  C=1           unsigned higher or same (carry set)
    Cond_CC, // CC/LO  C=0           unsigned lower (carry cleared)
    Cond_MI, // MI     N=1           signed negative (minus)
    Cond_PL, // PL     N=0           signed positive or zero (plus)
    Cond_VS, // VS     V=1           signed overflow (V set)
    Cond_VC, // VC     V=0           signed no overflow (V cleared)
    Cond_HI, // HI     C=1 and Z=0   unsigned higher
    Cond_LS, // LS     C=0 or Z=1    unsigned lower or same
    Cond_GE, // GE     N=V           signed greater or equal
    Cond_LT, // LT     N<>V          signed less than
    Cond_GT, // GT     Z=0 and N=V   signed greater than
    Cond_LE, // LE     Z=1 or N<>V   signed less or equal
    Cond_AL, // AL     -             always (the "AL" suffix can be omitted)
    Cond_NV, // NV     -             never (ARMv1,v2 only) (Reserved ARMv3 and up)

    Cond_HS = Cond_CS,
    Cond_LO = Cond_CC,
};

} // namespace interp::arm
