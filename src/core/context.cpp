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
    case CPUModel::ARM946ES: {
        using namespace arm::cp15::id;

        m_arch = CPUArch::ARMv5TE;
        auto &cp15 = m_armState.GetSystemControlCoprocessor();
        cp15.Install(Implementor::ARM, 0, Architecture::v5TE, kPrimaryPartNumberARM946, 1);
        cp15.GetTCM().memMap = const_cast<MemoryMap *>(&system.GetMemoryMap());
        break;
    }
    }
}

uint16_t Context::CodeReadHalf(uint32_t address) {
    auto &cp15 = m_armState.GetSystemControlCoprocessor();
    if (cp15.IsPresent()) {
        auto &tcm = cp15.GetTCM();
        if (address < tcm.itcmReadSize) {
            assert(std::popcount(tcm.itcmSize) == 1); // must be a power of two
            const uint32_t addrMask = (tcm.itcmSize - 1) & ~1;
            return *reinterpret_cast<uint16_t *>(&tcm.itcm[address & addrMask]);
        }
        if (address - tcm.dtcmBase < tcm.dtcmSize) {
            assert(std::popcount(tcm.dtcmSize) == 1); // must be a power of two
            const uint32_t addrMask = (tcm.dtcmSize - 1) & ~1;
            return *reinterpret_cast<uint16_t *>(&tcm.dtcm[address & addrMask]);
        }
    }
    return m_system.MemReadHalf(address);
}

uint32_t Context::CodeReadWord(uint32_t address) {
    auto &cp15 = m_armState.GetSystemControlCoprocessor();
    if (cp15.IsPresent()) {
        auto &tcm = cp15.GetTCM();
        if (address < tcm.itcmReadSize) {
            assert(std::popcount(tcm.itcmSize) == 1); // must be a power of two
            const uint32_t addrMask = (tcm.itcmSize - 1) & ~3;
            return *reinterpret_cast<uint32_t *>(&tcm.itcm[address & addrMask]);
        }
        if (address - tcm.dtcmBase < tcm.dtcmSize) {
            assert(std::popcount(tcm.dtcmSize) == 1); // must be a power of two
            const uint32_t addrMask = (tcm.dtcmSize - 1) & ~3;
            return *reinterpret_cast<uint32_t *>(&tcm.dtcm[address & addrMask]);
        }
    }
    return m_system.MemReadWord(address);
}

} // namespace armajitto
