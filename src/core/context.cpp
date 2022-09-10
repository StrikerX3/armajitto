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
        using namespace arm::cp15::id;

        m_arch = CPUArch::ARMv5TE;
        m_armState.GetSystemControlCoprocessor().Install(Implementor::ARM, 0, Architecture::v5TE,
                                                         kPrimaryPartNumberARM946, 1,
                                                         const_cast<MemoryMap &>(system.GetMemoryMap()));
        break;
    }
}

} // namespace armajitto
