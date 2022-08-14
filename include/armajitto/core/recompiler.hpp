#pragma once

#include "armajitto/arm/state.hpp"
#include "specification.hpp"

namespace armajitto {

class Recompiler {
public:
    Recompiler(const Specification &spec);

    arm::State &ARMState() {
        return m_armState;
    }

private:
    arm::State m_armState;
};

} // namespace armajitto
