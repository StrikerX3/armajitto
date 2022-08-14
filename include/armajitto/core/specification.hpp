#pragma once

#include "armajitto/defs/cpu_model.hpp"
#include "system_interface.hpp"

namespace armajitto {

struct Specification {
    ISystem &system;
    CPUModel cpuModel;
};

} // namespace armajitto
