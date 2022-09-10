#include "armajitto/guest/arm/coprocessors/coproc_15_sys_control.hpp"

namespace armajitto::arm {

void SystemControlCoprocessor::Reset() {
    m_ctl.Reset();
    m_pu.Reset();
    m_tcm.Reset();
    m_tcm.SetupITCM(m_ctl.value.itcmEnable, m_ctl.value.itcmLoad);
    m_tcm.SetupDTCM(m_ctl.value.dtcmEnable, m_ctl.value.dtcmLoad);
}

void SystemControlCoprocessor::Install(cp15::id::Implementor implementor, uint32_t variant,
                                       cp15::id::Architecture architecture, uint32_t primaryPartNumber,
                                       uint32_t revision, MemoryMap &memMap) {
    m_installed = true;
    m_id = {
        .implementor = implementor,
        .variant = variant,
        .architecture = architecture,
        .primaryPartNumber = primaryPartNumber,
        .revision = revision,
    };
    m_tcm.memMap = &memMap;
}

void SystemControlCoprocessor::Uninstall() {
    m_installed = false;
    m_tcm.Disable();
}

void SystemControlCoprocessor::ConfigureTCM(const cp15::TCM::Configuration &config) {
    m_tcm.Configure(config);
}

void SystemControlCoprocessor::ConfigureCache(const cp15::Cache::Configuration &config) {
    m_cache.Configure(config);
}

// ---------------------------------------------------------------------------------------------------------------------
// Coprocessor interface implementation

uint32_t SystemControlCoprocessor::LoadRegister(CopRegister reg) const {
    switch (reg.u16) {
    case 0x0000: // 0,C0,C0,0 - Main ID Register
    case 0x0003: // 0,C0,C0,3 - Reserved - copy of C0,C0,0
    case 0x0004: // 0,C0,C0,4 - Reserved - copy of C0,C0,0
    case 0x0005: // 0,C0,C0,5 - Reserved - copy of C0,C0,0
    case 0x0006: // 0,C0,C0,6 - Reserved - copy of C0,C0,0
    case 0x0007: // 0,C0,C0,7 - Reserved - copy of C0,C0,0
        return m_id.u32;
    case 0x0001: // 0,C0,C0,1 - Cache Type Register
        return m_cache.params.u32;
    case 0x0002: // 0,C0,C0,2 - Tightly Coupled Memory (TCM) Size Register
        return m_tcm.params.u32;

    case 0x0100: // 0,C1,C0,0 - Control Register
        return m_ctl.value.u32;

    case 0x0200: // 0,C2,C0,0 - Cachability Bits for Data/Unified Protection Region
        return m_pu.dataCachabilityBits;
    case 0x0201: // 0,C2,C0,1 - Cachability Bits for Instruction Protection Region
        return m_pu.codeCachabilityBits;
    case 0x0300: // 0,C3,C0,0 - Cache Write-Bufferability Bits for Data Protection Regions
        return m_pu.bufferabilityBits;

    case 0x0500: { // 0,C5,C0,0 - Access Permission Data/Unified Protection Region
        uint32_t value = 0;
        for (size_t i = 0; i < 8; i++) {
            value |= (m_pu.dataAccessPermissions & (0x3 << (i * 4))) >> (i * 2);
        }
        return value;
    }
    case 0x0501: { // 0,C5,C0,1 - Access Permission Instruction Protection Region
        uint32_t value = 0;
        for (size_t i = 0; i < 8; i++) {
            value |= (m_pu.codeAccessPermissions & (0x3 << (i * 4))) >> (i * 2);
        }
        return value;
    }
    case 0x0502: // 0,C5,C0,2 - Extended Access Permission Data/Unified Protection Region
        return m_pu.dataAccessPermissions;
    case 0x0503: // 0,C5,C0,3 - Extended Access Permission Instruction Protection Region
        return m_pu.codeAccessPermissions;

    case 0x0600: // 0,C6,C0,0 - Protection Unit Data/Unified Region 0
    case 0x0601: // 0,C6,C0,1 - Protection Unit Instruction Region 0
    case 0x0610: // 0,C6,C1,0 - Protection Unit Data/Unified Region 1
    case 0x0611: // 0,C6,C1,1 - Protection Unit Instruction Region 1
    case 0x0620: // 0,C6,C2,0 - Protection Unit Data/Unified Region 2
    case 0x0621: // 0,C6,C2,1 - Protection Unit Instruction Region 2
    case 0x0630: // 0,C6,C3,0 - Protection Unit Data/Unified Region 3
    case 0x0631: // 0,C6,C3,1 - Protection Unit Instruction Region 3
    case 0x0640: // 0,C6,C4,0 - Protection Unit Data/Unified Region 4
    case 0x0641: // 0,C6,C4,1 - Protection Unit Instruction Region 4
    case 0x0650: // 0,C6,C5,0 - Protection Unit Data/Unified Region 5
    case 0x0651: // 0,C6,C5,1 - Protection Unit Instruction Region 5
    case 0x0660: // 0,C6,C6,0 - Protection Unit Data/Unified Region 6
    case 0x0661: // 0,C6,C6,1 - Protection Unit Instruction Region 6
    case 0x0670: // 0,C6,C7,0 - Protection Unit Data/Unified Region 7
    case 0x0671: // 0,C6,C7,1 - Protection Unit Instruction Region 7
        return m_pu.regions[reg.crm].u32;

    case 0x0910: // 0,C9,C1,0 - Data TCM Size/Base
        return m_tcm.dtcmParams;
    case 0x0911: // 0,C9,C1,1 - Instruction TCM Size/Base
        return m_tcm.itcmParams;

    default: return 0;
    }
}

void SystemControlCoprocessor::StoreRegister(CopRegister reg, uint32_t value) {
    switch (reg.u16) {
    case 0x0100: { // 0,C1,C0,0 - Control Register
        m_ctl.Write(value);
        m_tcm.SetupITCM(m_ctl.value.itcmEnable, m_ctl.value.itcmLoad);
        m_tcm.SetupDTCM(m_ctl.value.dtcmEnable, m_ctl.value.dtcmLoad);
        // TODO: UpdateTimingMaps();
        // TODO: UpdatePermissionMaps();
        break;
    }

    case 0x0200: { // 0,C2,C0,0 - Cachability Bits for Data/Unified Protection Region
        m_pu.dataCachabilityBits = value;
        // TODO: UpdateTimingMaps();
        break;
    }
    case 0x0201: { // 0,C2,C0,1 - Cachability Bits for Instruction Protection Region
        m_pu.codeCachabilityBits = value;
        // TODO: UpdateTimingMaps();
        break;
    }
    case 0x0300: { // 0,C3,C0,0 - Cache Write-Bufferability Bits for Data Protection Regions
        m_pu.bufferabilityBits = value;
        break;
    }

    case 0x0500: { // 0,C5,C0,0 - Access Permission Data/Unified Protection Region
        auto &bits = m_pu.dataAccessPermissions;
        bits = 0;
        for (size_t i = 0; i < 8; i++) {
            bits |= (value & (0x3 << (i * 2))) << (i * 2);
        }
        // TODO: UpdatePermissionMaps();
        break;
    }
    case 0x0501: { // 0,C5,C0,1 - Access Permission Instruction Protection Region
        auto &bits = m_pu.codeAccessPermissions;
        bits = 0;
        for (size_t i = 0; i < 8; i++) {
            bits |= (value & (0x3 << (i * 2))) << (i * 2);
        }
        // TODO: UpdatePermissionMaps();
        break;
    }
    case 0x0502: { // 0,C5,C0,2 - Extended Access Permission Data/Unified Protection Region
        m_pu.dataAccessPermissions = value;
        // TODO: UpdatePermissionMaps();
        break;
    }
    case 0x0503: { // 0,C5,C0,3 - Extended Access Permission Instruction Protection Region
        m_pu.codeAccessPermissions = value;
        // TODO: UpdatePermissionMaps();
        break;
    }

    case 0x0600: // 0,C6,C0,0 - Protection Unit Data/Unified Region 0
    case 0x0601: // 0,C6,C0,1 - Protection Unit Instruction Region 0
    case 0x0610: // 0,C6,C1,0 - Protection Unit Data/Unified Region 1
    case 0x0611: // 0,C6,C1,1 - Protection Unit Instruction Region 1
    case 0x0620: // 0,C6,C2,0 - Protection Unit Data/Unified Region 2
    case 0x0621: // 0,C6,C2,1 - Protection Unit Instruction Region 2
    case 0x0630: // 0,C6,C3,0 - Protection Unit Data/Unified Region 3
    case 0x0631: // 0,C6,C3,1 - Protection Unit Instruction Region 3
    case 0x0640: // 0,C6,C4,0 - Protection Unit Data/Unified Region 4
    case 0x0641: // 0,C6,C4,1 - Protection Unit Instruction Region 4
    case 0x0650: // 0,C6,C5,0 - Protection Unit Data/Unified Region 5
    case 0x0651: // 0,C6,C5,1 - Protection Unit Instruction Region 5
    case 0x0660: // 0,C6,C6,0 - Protection Unit Data/Unified Region 6
    case 0x0661: // 0,C6,C6,1 - Protection Unit Instruction Region 6
    case 0x0670: // 0,C6,C7,0 - Protection Unit Data/Unified Region 7
    case 0x0671: // 0,C6,C7,1 - Protection Unit Instruction Region 7
        m_pu.regions[reg.crm].u32 = value;
        // TODO: UpdateTimingMaps();
        // TODO: UpdatePermissionMaps();
        break;

    case 0x0704: // 0,C7,C0,4 - Wait For Interrupt (Halt)
    case 0x0782: // 0,C7,C8,2 - Wait For Interrupt (Halt), alternately to C7,C0,4
        m_execState = ExecState::Halted;
        break;

    case 0x0750: // 0,C7,C5,0 - Invalidate Entire Instruction Cache
        if (m_invalidateCodeCacheCallback != nullptr) {
            m_invalidateCodeCacheCallback(0, 0xFFFFFFFF, m_invalidateCodeCacheCallbackCtx);
        }
        break;
    case 0x0751: // 0,C7,C5,1 - Invalidate Instruction Cache Line
        if (m_invalidateCodeCacheCallback != nullptr) {
            uint32_t start = (value & ~0x1F);
            uint32_t end = start + 0x1F;
            m_invalidateCodeCacheCallback(start, end, m_invalidateCodeCacheCallbackCtx);
        }
        break;
    case 0x0752: // 0,C7,C5,2 - Invalidate Instruction Cache Line
        // TODO: implement
        break;

    case 0x0760: // 0,C7,C6,0 - Invalidate Entire Data Cache
        // TODO: implement
        break;
    case 0x0761: // 0,C7,C6,1 - Invalidate Data Cache Line
        // TODO: implement
        break;
    case 0x0762: // 0,C7,C6,2 - Invalidate Data Cache Line
        // TODO: implement
        break;

    case 0x07A1: // 0,C7,C10,1 - Clean Data Cache Line
        // TODO: implement
        break;
    case 0x07A2: // 0,C7,C10,2 - Clean Data Cache Line
        // TODO: implement
        break;

    case 0x0910: // 0,C9,C1,0 - Data TCM Size/Base
        m_tcm.dtcmParams = value;
        m_tcm.SetupDTCM(m_ctl.value.dtcmEnable, m_ctl.value.dtcmLoad);
        // TODO: UpdateTimingMaps();
        break;
    case 0x0911: // 0,C9,C1,1 - Instruction TCM Size/Base
        m_tcm.itcmParams = value;
        m_tcm.SetupITCM(m_ctl.value.itcmEnable, m_ctl.value.itcmLoad);
        // TODO: UpdateTimingMaps();
        break;
    }
}

bool SystemControlCoprocessor::RegStoreHasSideEffects(CopRegister reg) const {
    switch (reg.u16) {
    case 0x0704: // 0,C7,C0,4 - Wait For Interrupt (Halt)
    case 0x0782: // 0,C7,C8,2 - Wait For Interrupt (Halt), alternately to C7,C0,4
    case 0x0750: // 0,C7,C5,0 - Invalidate Entire Instruction Cache
    case 0x0751: // 0,C7,C5,1 - Invalidate Instruction Cache Line
        return true;
    default: return false;
    }
}

} // namespace armajitto::arm
