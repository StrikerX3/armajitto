#pragma once

#include "armajitto/guest/arm/exceptions.hpp"
#include "armajitto/guest/arm/mode.hpp"

#include <cstdint>

namespace armajitto::arm {

struct ExceptionVectorInfo {
    Mode mode;            // Mode on entry
    bool F;               // true: F=1, false: F=unchanged
    uint32_t armOffset;   // Additional offset in bytes from PC (ARM instructions)
    uint32_t thumbOffset; // Additional offset in bytes from PC (THUMB instructions)
};

constexpr ExceptionVectorInfo kExceptionVectorInfos[] = {
    {Mode::Supervisor, true, 0, 0},  // [BASE+00h] Reset
    {Mode::Undefined, false, 4, 2},  // [BASE+04h] Undefined Instruction
    {Mode::Supervisor, false, 4, 2}, // [BASE+08h] Software Interrupt (SWI)
    {Mode::Abort, false, 4, 4},      // [BASE+0Ch] Prefetch Abort
    {Mode::Abort, false, 8, 8},      // [BASE+10h] Data Abort
    {Mode::Supervisor, false, 4, 2}, // [BASE+14h] Address Exceeds 26bit
    {Mode::IRQ, false, 4, 4},        // [BASE+18h] Normal Interrupt (IRQ)
    {Mode::FIQ, true, 4, 4},         // [BASE+1Ch] Fast Interrupt (FIQ)
};

} // namespace armajitto::arm
