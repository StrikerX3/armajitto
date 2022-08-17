#pragma once

#include "armajitto/defs/cpu_arch.hpp"
#include "system_interface.hpp"

namespace armajitto {

struct Specification {
    ISystem &system;
    CPUArch arch;
};

} // namespace armajitto