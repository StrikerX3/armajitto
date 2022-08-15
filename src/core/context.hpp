#pragma once

#include "arm/decoder.hpp"
#include "armajitto/arm/state.hpp"
#include "armajitto/core/system_interface.hpp"

namespace armajitto {

class Context {
public:
    Context(CPUArch arch, ISystem &system)
        : m_arch(arch)
        , m_system(system) {}

    arm::State &GetARMState() {
        return m_armState;
    }

    // --- Decoder client implementation helpers -------------------------------

    CPUArch GetCPUArch() const {
        return m_arch;
    }

    uint16_t CodeReadHalf(uint32_t address) {
        // TODO: handle TCM
        return m_system.ReadHalf(address);
    }

    uint32_t CodeReadWord(uint32_t address) {
        // TODO: handle TCM
        return m_system.ReadWord(address);
    }

private:
    CPUArch m_arch;
    ISystem &m_system;
    arm::State m_armState;
};

} // namespace armajitto
