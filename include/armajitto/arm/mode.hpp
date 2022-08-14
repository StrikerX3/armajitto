#pragma once

#include <cstdint>

namespace armajitto {

enum class Mode : uint32_t {
    User = 0x10,
    FIQ = 0x11,
    IRQ = 0x12,
    Supervisor = 0x13, // aka SWI
    Abort = 0x17,
    Undefined = 0x1B,
    System = 0x1F
};

} // namespace armajitto
