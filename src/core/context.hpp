#pragma once

#include "armajitto/arm/state.hpp"
#include "armajitto/core/system_interface.hpp"
#include "armajitto/defs/cpu_arch.hpp"

namespace armajitto {

class Context {
public:
    Context(CPUArch arch, ISystem &system)
        : m_arch(arch)
        , m_system(system) {}

    arm::State &GetARMState() {
        return m_armState;
    }

    // --- Code accessor implementation ----------------------------------------

    CPUArch GetCPUArch() const {
        return m_arch;
    }

    uint16_t CodeReadHalf(uint32_t address) {
        // TODO: handle TCM
        return m_system.MemReadHalf(address);
    }

    uint32_t CodeReadWord(uint32_t address) {
        // TODO: handle TCM
        return m_system.MemReadWord(address);
    }

private:
    CPUArch m_arch;
    ISystem &m_system;
    arm::State m_armState;
};

} // namespace armajitto
