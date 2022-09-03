#pragma once

#include "armajitto/defs/cpu_model.hpp"
#include "system_interface.hpp"

namespace armajitto {

struct Specification {
    ISystem &system;
    CPUModel model;
    std::size_t maxHostCodeSize = 32u * 1024u * 1024u;
};

} // namespace armajitto
