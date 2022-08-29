#pragma once

#include "armajitto/core/system_interface.hpp"
#include "armajitto/defs/cpu_arch.hpp"
#include "armajitto/defs/cpu_model.hpp"
#include "armajitto/guest/arm/coprocessor.hpp"
#include "armajitto/guest/arm/coprocessors/coproc_null.hpp"
#include "armajitto/guest/arm/state.hpp"

namespace armajitto {

class Context {
public:
    Context(CPUModel model, ISystem &system);

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

    uint16_t CodeReadHalf(uint32_t address) {
        // TODO: handle TCM
        return m_system.MemReadHalf(address);
    }

    uint32_t CodeReadWord(uint32_t address) {
        // TODO: handle TCM
        return m_system.MemReadWord(address);
    }

    arm::Coprocessor &GetCoprocessor(uint8_t cpnum) {
        return m_armState.GetCoprocessor(cpnum);
    }

private:
    arm::State m_armState;

    CPUArch m_arch;
    ISystem &m_system;
};

} // namespace armajitto
