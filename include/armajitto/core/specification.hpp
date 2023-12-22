#pragma once

#include "armajitto/defs/cpu_model.hpp"
#include "system_interface.hpp"

namespace armajitto {

struct Specification {
    ISystem &system;
    CPUModel model;

    // Specifies a pointer to a 64-bit cycle counter that serves as the deadline until the next event.
    // This affects the behavior of the recompiler's Run(cycles) method as follows:
    // - If this pointer is not specified (nullptr), the Run(cycles) method will run for a minimum of the specified
    //   number of cycles
    // - If this pointer is specified, Run(cycles) will use its argument as the initial cycle count and will execute
    //   until it reaches the cycle count given by this value
    // Pointing this to a variable is useful in scenarios where the next deadline might change while JITted code is
    // executed, requiring an early break out of a block.
    const uint64_t *cycleCountDeadline = nullptr;
};

} // namespace armajitto
