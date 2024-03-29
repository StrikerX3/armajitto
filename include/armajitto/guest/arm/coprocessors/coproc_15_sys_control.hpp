#pragma once

#include "armajitto/guest/arm/coprocessor.hpp"
#include "armajitto/guest/arm/exec_state.hpp"

#include "cp15/cp15_cache.hpp"
#include "cp15/cp15_control.hpp"
#include "cp15/cp15_defs.hpp"
#include "cp15/cp15_id.hpp"
#include "cp15/cp15_pu.hpp"
#include "cp15/cp15_tcm.hpp"

#include <bit>
#include <vector>

namespace armajitto {

class Host;

} // namespace armajitto

namespace armajitto::arm {

using InvalidateCodeCacheCallback = void (*)(uint32_t start, uint32_t end, void *ctx);

class SystemControlCoprocessor : public Coprocessor {
public:
    SystemControlCoprocessor(ExecState &execState)
        : m_execState(execState) {}

    void Reset();

    // Installs the coprocessor and configures the ID returned by the ID codes register (0).
    // This implementation only supports the post-ARM7 processors format:
    //  31         24 23     20 19          16 15                  4 3        0
    // | Implementor | Variant | Architecture | Primary part number | Revision |
    void Install(cp15::id::Implementor implementor, uint32_t variant, cp15::id::Architecture architecture,
                 uint32_t primaryPartNumber, uint32_t revision, MemoryMap &memMap);

    // Uninstalls the coprocessor, freeing up TCM memory.
    void Uninstall();

    // Configures the TCM with the specified parameters.
    // TCM memory sizes are rounded up to the next power of two not less than the value.
    // A size of 0 disables the specified TCM region.
    void ConfigureTCM(const cp15::TCM::Configuration &config);

    // Configures the cache with the specified parameters.
    void ConfigureCache(const cp15::Cache::Configuration &config);

    const cp15::Identification &GetIdentification() const {
        return m_id;
    }

    const cp15::ControlRegister &GetControlRegister() const {
        return m_ctl;
    }

    const cp15::ProtectionUnit &GetProtectionUnit() const {
        return m_pu;
    }

    const cp15::TCM &GetTCM() const {
        return m_tcm;
    }

    const cp15::Cache &GetCache() const {
        return m_cache;
    }

    // -------------------------------------------------------------------------
    // Coprocessor interface implementation

    bool IsPresent() const final {
        return m_installed;
    }

    uint32_t LoadRegister(CopRegister reg) const final;
    void StoreRegister(CopRegister reg, uint32_t value) final;
    bool RegStoreHasSideEffects(CopRegister reg) const final;

private:
    bool m_installed = false;

    ExecState &m_execState;

    InvalidateCodeCacheCallback m_invalidateCodeCacheCallback = nullptr;
    void *m_invalidateCodeCacheCallbackCtx = nullptr;

    cp15::Identification m_id;
    cp15::ControlRegister m_ctl;
    cp15::ProtectionUnit m_pu;
    cp15::TCM m_tcm;
    cp15::Cache m_cache;

    // Gives hosts access to the callback fields above.
    struct PrivateAccess;
    friend class armajitto::Host;
};

} // namespace armajitto::arm
