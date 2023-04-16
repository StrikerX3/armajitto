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
    m_id.implementor = implementor;
    m_id.variant = variant;
    m_id.architecture = architecture;
    m_id.primaryPartNumber = primaryPartNumber;
    m_id.revision = revision;
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
    case 0x0000: // 0,C0,C0,0 - Main ID register
    case 0x0003: // 0,C0,C0,3 - Reserved - copy of C0,C0,0
    case 0x0004: // 0,C0,C0,4 - Reserved - copy of C0,C0,0
    case 0x0005: // 0,C0,C0,5 - Reserved - copy of C0,C0,0
    case 0x0006: // 0,C0,C0,6 - Reserved - copy of C0,C0,0
    case 0x0007: // 0,C0,C0,7 - Reserved - copy of C0,C0,0
        return m_id.u32;
    case 0x0001: // 0,C0,C0,1 - Cache type register
        return m_cache.params.u32;
    case 0x0002: // 0,C0,C0,2 - Tightly Coupled Memory (TCM) size register
        return m_tcm.params.u32;

    case 0x0100: // 0,C1,C0,0 - Control Register
        return m_ctl.value.u32;

    case 0x0200: // 0,C2,C0,0 - Cachability bits for data/unified protection region
        return m_pu.dataCachabilityBits;
    case 0x0201: // 0,C2,C0,1 - Cachability bits for instruction protection region
        return m_pu.codeCachabilityBits;
    case 0x0300: // 0,C3,C0,0 - Cache write-bufferability bits for data protection regions
        return m_pu.bufferabilityBits;

    case 0x0500: { // 0,C5,C0,0 - Access permission data/unified protection region
        uint32_t value = 0;
        for (size_t i = 0; i < 8; i++) {
            value |= (m_pu.dataAccessPermissions & (0x3 << (i * 4))) >> (i * 2);
        }
        return value;
    }
    case 0x0501: { // 0,C5,C0,1 - Access permission instruction protection region
        uint32_t value = 0;
        for (size_t i = 0; i < 8; i++) {
            value |= (m_pu.codeAccessPermissions & (0x3 << (i * 4))) >> (i * 2);
        }
        return value;
    }
    case 0x0502: // 0,C5,C0,2 - Extended access permission data/unified protection region
        return m_pu.dataAccessPermissions;
    case 0x0503: // 0,C5,C0,3 - Extended access permission instruction protection region
        return m_pu.codeAccessPermissions;

    case 0x0600: // 0,C6,C0,0 - Protection unit data/unified region 0
    case 0x0601: // 0,C6,C0,1 - Protection unit instruction region 0
    case 0x0610: // 0,C6,C1,0 - Protection unit data/unified region 1
    case 0x0611: // 0,C6,C1,1 - Protection unit instruction region 1
    case 0x0620: // 0,C6,C2,0 - Protection unit data/unified region 2
    case 0x0621: // 0,C6,C2,1 - Protection unit instruction region 2
    case 0x0630: // 0,C6,C3,0 - Protection unit data/unified region 3
    case 0x0631: // 0,C6,C3,1 - Protection unit instruction region 3
    case 0x0640: // 0,C6,C4,0 - Protection unit data/unified region 4
    case 0x0641: // 0,C6,C4,1 - Protection unit instruction region 4
    case 0x0650: // 0,C6,C5,0 - Protection unit data/unified region 5
    case 0x0651: // 0,C6,C5,1 - Protection unit instruction region 5
    case 0x0660: // 0,C6,C6,0 - Protection unit data/unified region 6
    case 0x0661: // 0,C6,C6,1 - Protection unit instruction region 6
    case 0x0670: // 0,C6,C7,0 - Protection unit data/unified region 7
    case 0x0671: // 0,C6,C7,1 - Protection unit instruction region 7
        return m_pu.regions[reg.crm].u32;

    case 0x0900: // 0,C9,C0,0 - Data cache lockdown register
        // TODO: implement
        return 0;
    case 0x0901: // 0,C9,C0,1 - Instruction cache lockdown register
        // TODO: implement
        return 0;
    case 0x0910: // 0,C9,C1,0 - Data TCM size/base
        return m_tcm.dtcmParams;
    case 0x0911: // 0,C9,C1,1 - Instruction TCM size/base
        return m_tcm.itcmParams;

    case 0x0D01: // 0,C13,C0,1 - Trace process ID
    case 0x0D11: // 0,C13,C1,1 - Trace process ID
        // TODO: implement
        return 0;

    case 0x0F00: // 0,C15,C0,0 - Test state register
        // TODO: implement
        return 0;

    case 0x0F01: // 0,C15,C0,1 - TAG BIST control register
        // TODO: implement
        return 0;
    case 0x0F02: // 0,C15,C0,2 - Instruction TAG BIST address register
        // TODO: implement
        return 0;
    case 0x0F03: // 0,C15,C0,3 - Instruction TAG BIST general register
        // TODO: implement
        return 0;
    case 0x0F06: // 0,C15,C0,6 - Data TAG BIST address register
        // TODO: implement
        return 0;
    case 0x0F07: // 0,C15,C0,7 - Data TAG BIST general register
        // TODO: implement
        return 0;

    case 0x1F01: // 1,C15,C0,1 - TCM BIST control register
        // TODO: implement
        return 0;
    case 0x1F02: // 1,C15,C0,2 - Instruction TCM BIST address register
        // TODO: implement
        return 0;
    case 0x1F03: // 1,C15,C0,3 - Instruction TCM BIST general register
        // TODO: implement
        return 0;
    case 0x1F06: // 1,C15,C0,6 - Data TCM BIST address register
        // TODO: implement
        return 0;
    case 0x1F07: // 1,C15,C0,7 - Data TCM BIST general register
        // TODO: implement
        return 0;

    case 0x1F10: // 1,C15,C1,0 - Trace control register
        // TODO: implement
        return 0;

    case 0x2F01: // 2,C15,C0,1 - Cache RAM BIST control register
        // TODO: implement
        return 0;
    case 0x2F02: // 2,C15,C0,2 - Instruction cache RAM BIST address register
        // TODO: implement
        return 0;
    case 0x2F03: // 2,C15,C0,3 - Instruction cache RAM BIST general register
        // TODO: implement
        return 0;
    case 0x2F06: // 2,C15,C0,6 - Data cache RAM BIST address register
        // TODO: implement
        return 0;
    case 0x2F07: // 2,C15,C0,7 - Data cache RAM BIST general register
        // TODO: implement
        return 0;

    case 0x3F00: // 3,C15,C0,0 - Cache debug index register
        // TODO: implement
        return 0;
    case 0x3F10: // 3,C15,C1,0 - Instruction TAG access
        // TODO: implement
        return 0;
    case 0x3F20: // 3,C15,C2,0 - Data TAG access
        // TODO: implement
        return 0;
    case 0x3F30: // 3,C15,C3,0 - Instruction cache access
        // TODO: implement
        return 0;
    case 0x3F40: // 3,C15,C4,0 - Data cache access
        // TODO: implement
        return 0;

    default: return 0;
    }
}

void SystemControlCoprocessor::StoreRegister(CopRegister reg, uint32_t value) {
    switch (reg.u16) {
    case 0x0100: { // 0,C1,C0,0 - Control register
        m_ctl.Write(value);
        m_tcm.SetupITCM(m_ctl.value.itcmEnable, m_ctl.value.itcmLoad);
        m_tcm.SetupDTCM(m_ctl.value.dtcmEnable, m_ctl.value.dtcmLoad);
        // TODO: UpdateTimingMaps();
        // TODO: UpdatePermissionMaps();
        break;
    }

    case 0x0200: { // 0,C2,C0,0 - Cachability bits for data/unified protection region
        m_pu.dataCachabilityBits = value;
        // TODO: UpdateTimingMaps();
        break;
    }
    case 0x0201: { // 0,C2,C0,1 - Cachability bits for instruction protection region
        m_pu.codeCachabilityBits = value;
        // TODO: UpdateTimingMaps();
        break;
    }
    case 0x0300: { // 0,C3,C0,0 - Cache write-bufferability bits for data protection regions
        m_pu.bufferabilityBits = value;
        break;
    }

    case 0x0500: { // 0,C5,C0,0 - Access permission data/unified protection region
        auto &bits = m_pu.dataAccessPermissions;
        bits = 0;
        for (size_t i = 0; i < 8; i++) {
            bits |= (value & (0x3 << (i * 2))) << (i * 2);
        }
        // TODO: UpdatePermissionMaps();
        break;
    }
    case 0x0501: { // 0,C5,C0,1 - Access permission instruction protection region
        auto &bits = m_pu.codeAccessPermissions;
        bits = 0;
        for (size_t i = 0; i < 8; i++) {
            bits |= (value & (0x3 << (i * 2))) << (i * 2);
        }
        // TODO: UpdatePermissionMaps();
        break;
    }
    case 0x0502: { // 0,C5,C0,2 - Extended access permission data/unified protection region
        m_pu.dataAccessPermissions = value;
        // TODO: UpdatePermissionMaps();
        break;
    }
    case 0x0503: { // 0,C5,C0,3 - Extended access permission instruction protection region
        m_pu.codeAccessPermissions = value;
        // TODO: UpdatePermissionMaps();
        break;
    }

    case 0x0600: // 0,C6,C0,0 - Protection unit data/unified region 0
    case 0x0601: // 0,C6,C0,1 - Protection unit instruction region 0
    case 0x0610: // 0,C6,C1,0 - Protection unit data/unified region 1
    case 0x0611: // 0,C6,C1,1 - Protection unit instruction region 1
    case 0x0620: // 0,C6,C2,0 - Protection unit data/unified region 2
    case 0x0621: // 0,C6,C2,1 - Protection unit instruction region 2
    case 0x0630: // 0,C6,C3,0 - Protection unit data/unified region 3
    case 0x0631: // 0,C6,C3,1 - Protection unit instruction region 3
    case 0x0640: // 0,C6,C4,0 - Protection unit data/unified region 4
    case 0x0641: // 0,C6,C4,1 - Protection unit instruction region 4
    case 0x0650: // 0,C6,C5,0 - Protection unit data/unified region 5
    case 0x0651: // 0,C6,C5,1 - Protection unit instruction region 5
    case 0x0660: // 0,C6,C6,0 - Protection unit data/unified region 6
    case 0x0661: // 0,C6,C6,1 - Protection unit instruction region 6
    case 0x0670: // 0,C6,C7,0 - Protection unit data/unified region 7
    case 0x0671: // 0,C6,C7,1 - Protection unit instruction region 7
        m_pu.regions[reg.crm].u32 = value;
        // TODO: UpdateTimingMaps();
        // TODO: UpdatePermissionMaps();
        break;

    case 0x0704: // 0,C7,C0,4 - Wait for interrupt
    case 0x0782: // 0,C7,C8,2 - Wait for interrupt (deprecated encoding)
        m_execState = ExecState::Halted;
        break;

    case 0x0750: // 0,C7,C5,0 - Invalidate entire instruction cache
        if (m_invalidateCodeCacheCallback != nullptr) {
            m_invalidateCodeCacheCallback(0, 0xFFFFFFFF, m_invalidateCodeCacheCallbackCtx);
        }
        break;
    case 0x0751: // 0,C7,C5,1 - Invalidate instruction cache line (by address)
        if (m_invalidateCodeCacheCallback != nullptr) {
            uint32_t start = (value & ~0x1F);
            uint32_t end = start + 0x1F;
            m_invalidateCodeCacheCallback(start, end, m_invalidateCodeCacheCallbackCtx);
        }
        break;
    case 0x0752: // 0,C7,C5,2 - Invalidate instruction cache line (by set and index)
        // TODO: implement
        break;
    case 0x0754: // 0,C7,C5,4 - Flush prefetch buffer
        // TODO: implement
        break;
    case 0x0756: // 0,C7,C5,6 - Flush entire branch target cache
        // TODO: implement
        break;
    case 0x0757: // 0,C7,C5,7 - Flush branch target cache entry
        // TODO: implement
        break;

    case 0x0760: // 0,C7,C6,0 - Invalidate entire data cache
        // TODO: implement
        break;
    case 0x0761: // 0,C7,C6,1 - Invalidate data cache line (by address)
        // TODO: implement
        break;
    case 0x0762: // 0,C7,C6,2 - Invalidate data cache line (by set and index)
        // TODO: implement
        break;

    case 0x0770: // 0,C7,C7,0 - Invalidate entire unified cache or both instruction and data caches
        // TODO: implement
        break;
    case 0x0771: // 0,C7,C7,1 - Invalidate unified cache line (by address)
        // TODO: implement
        break;
    case 0x0772: // 0,C7,C7,2 - Invalidate unified cache line (by set and index)
        // TODO: implement
        break;

    case 0x07A1: // 0,C7,C10,1 - Clean data cache line (by address)
        // TODO: implement
        break;
    case 0x07A2: // 0,C7,C10,2 - Clean data cache line (by set and index)
        // TODO: implement
        break;
    case 0x07A4: // 0,C7,C10,4 - Drain write buffer
        // TODO: implement
        break;

    case 0x07B1: // 0,C7,C11,1 - Clean unified cache line (by address)
        // TODO: implement
        break;
    case 0x07B2: // 0,C7,C11,2 - Clean unified cache line (by set and index)
        // TODO: implement
        break;

    case 0x07D1: // 0,C7,C13,1 - Prefetch instruction cache line (by address)
        // TODO: implement
        break;

    case 0x07E1: // 0,C7,C14,1 - Clean and invalidate data cache line (by address)
        // TODO: implement
        break;
    case 0x07E2: // 0,C7,C14,2 - Clean and invalidate data cache line (by set and index)
        // TODO: implement
        break;

    case 0x07F1: // 0,C7,C15,1 - Clean and invalidate unified cache line (by address)
        // TODO: implement
        break;
    case 0x07F2: // 0,C7,C15,2 - Clean and invalidate unified cache line (by set and index)
        // TODO: implement
        break;

    case 0x0900: // 0,C9,C0,0 - Data cache lockdown register
        // TODO: implement
        break;
    case 0x0901: // 0,C9,C0,1 - Instruction cache lockdown register
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

    case 0x0D01: // 0,C13,C0,1 - Trace process ID
    case 0x0D11: // 0,C13,C1,1 - Trace process ID
        // TODO: implement
        break;

    case 0x0F00: // 0,C15,C0,0 - Test state register
        // TODO: implement
        break;

    case 0x0F01: // 0,C15,C0,1 - TAG BIST control register
        // TODO: implement
        break;
    case 0x0F02: // 0,C15,C0,2 - Instruction TAG BIST address register
        // TODO: implement
        break;
    case 0x0F03: // 0,C15,C0,3 - Instruction TAG BIST general register
        // TODO: implement
        break;
    case 0x0F06: // 0,C15,C0,6 - Data TAG BIST address register
        // TODO: implement
        break;
    case 0x0F07: // 0,C15,C0,7 - Data TAG BIST general register
        // TODO: implement
        break;

    case 0x1F01: // 1,C15,C0,1 - TCM BIST control register
        // TODO: implement
        break;
    case 0x1F02: // 1,C15,C0,2 - Instruction TCM BIST address register
        // TODO: implement
        break;
    case 0x1F03: // 1,C15,C0,3 - Instruction TCM BIST general register
        // TODO: implement
        break;
    case 0x1F06: // 1,C15,C0,6 - Data TCM BIST address register
        // TODO: implement
        break;
    case 0x1F07: // 1,C15,C0,7 - Data TCM BIST general register
        // TODO: implement
        break;

    case 0x1F10: // 1,C15,C1,0 - Trace control register
        // TODO: implement
        break;

    case 0x2F01: // 2,C15,C0,1 - Cache RAM BIST control register
        // TODO: implement
        break;
    case 0x2F02: // 2,C15,C0,2 - Instruction cache RAM BIST address register
        // TODO: implement
        break;
    case 0x2F03: // 2,C15,C0,3 - Instruction cache RAM BIST general register
        // TODO: implement
        break;
    case 0x2F06: // 2,C15,C0,6 - Data cache RAM BIST address register
        // TODO: implement
        break;
    case 0x2F07: // 2,C15,C0,7 - Data cache RAM BIST general register
        // TODO: implement
        break;

    case 0x3F00: // 3,C15,C0,0 - Cache debug index register
        // TODO: implement
        break;
    case 0x3F10: // 3,C15,C1,0 - Instruction TAG access
        // TODO: implement
        break;
    case 0x3F20: // 3,C15,C2,0 - Data TAG access
        // TODO: implement
        break;
    case 0x3F30: // 3,C15,C3,0 - Instruction cache access
        // TODO: implement
        break;
    case 0x3F40: // 3,C15,C4,0 - Data cache access
        // TODO: implement
        break;
    }
}

bool SystemControlCoprocessor::RegStoreHasSideEffects(CopRegister reg) const {
    switch (reg.u16) {
    case 0x0704: // 0,C7,C0,4 - Wait for interrupt
    case 0x0782: // 0,C7,C8,2 - Wait for interrupt (deprecated encoding)
    case 0x0750: // 0,C7,C5,0 - Invalidate entire instruction cache
    case 0x0751: // 0,C7,C5,1 - Invalidate instruction cache line (by address)
        return true;
    default: return false;
    }
}

} // namespace armajitto::arm
