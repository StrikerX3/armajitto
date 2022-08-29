#include "armajitto/core/context.hpp"

#include "armajitto/guest/arm/coprocessors/cp15/cp15_defs.hpp"

namespace armajitto {

Context::Context(CPUModel model, ISystem &system)
    : m_system(system) {
    switch (model) {
    case CPUModel::ARM7TDMI:
        m_arch = CPUArch::ARMv4T;
        m_armState.GetDummyDebugCoprocessor().Install();
        break;
    case CPUModel::ARM946ES:
        m_arch = CPUArch::ARMv5TE;
        m_armState.GetSystemControlCoprocessor().Install();
        m_armState.GetSystemControlCoprocessor().ConfigureID(arm::cp15::id::Implementor::ARM, 0,
                                                             arm::cp15::id::Architecture::v5TE,
                                                             arm::cp15::id::kPrimaryPartNumberARM946, 1);
        break;
    }
}

uint16_t Context::CodeReadHalf(uint32_t address) {
    // TODO: handle TCM
    return m_system.MemReadHalf(address);
}

uint32_t Context::CodeReadWord(uint32_t address) {
    // TODO: handle TCM
    return m_system.MemReadWord(address);
}

} // namespace armajitto
