#pragma once

#include "armajitto/core/system_interface.hpp"
#include "armajitto/defs/arm/state.hpp"
#include "armajitto/defs/cpu_arch.hpp"

namespace armajitto {

class Context {
public:
    Context(CPUArch arch, ISystem &system)
        : m_arch(arch)
        , m_system(system) {}

    CPUArch GetCPUArch() const {
        return m_arch;
    }

    ISystem &GetSystem() {
        return m_system;
    }

    const ISystem &GetSystem() const {
        return m_system;
    }

    arm::State &GetARMState() {
        return m_armState;
    }

    const arm::State &GetARMState() const {
        return m_armState;
    }

private:
    CPUArch m_arch;
    ISystem &m_system;
    arm::State m_armState;
};

} // namespace armajitto
