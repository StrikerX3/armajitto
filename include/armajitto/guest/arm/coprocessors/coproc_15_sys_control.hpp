#pragma once

#include "armajitto/guest/arm/coprocessor.hpp"

#include "cp15/cp15_cache.hpp"
#include "cp15/cp15_control.hpp"
#include "cp15/cp15_defs.hpp"
#include "cp15/cp15_id.hpp"
#include "cp15/cp15_pu.hpp"
#include "cp15/cp15_tcm.hpp"

#include <bit>
#include <vector>

namespace armajitto::arm {

class SystemControlCoprocessor : public Coprocessor {
public:
    SystemControlCoprocessor() = default;

    void Reset();

    // Installs the coprocessor.
    void Install();

    // Uninstalls the coprocessor, freeing up TCM memory.
    void Uninstall();

    // Configures the ID returned by the ID codes register (0).
    // This implementation only supports the post-ARM7 processors format:
    //  31         24 23     20 19          16 15                  4 3        0
    // | Implementor | Variant | Architecture | Primary part number | Revision |
    void ConfigureID(cp15::id::Implementor implementor, uint32_t variant, cp15::id::Architecture architecture,
                     uint32_t primaryPartNumber, uint32_t revision);

    // Configures the TCM with the specified parameters.
    // TCM memory sizes are rounded up to the next power of two not less than the value.
    // A size of 0 disables the specified TCM region.
    void ConfigureTCM(const cp15::TCM::Configuration &config);

    // Configures the cache with the specified parameters.
    void ConfigureCache(const cp15::Cache::Configuration &config);

    // -------------------------------------------------------------------------
    // Coprocessor interface implementation

    bool IsPresent() final {
        return m_installed;
    }

    bool SupportsExtendedRegTransfers() final {
        return false;
    }

    uint32_t LoadRegister(CopRegister reg) final;
    void StoreRegister(CopRegister reg, uint32_t value) final;
    bool RegStoreHasSideEffects(CopRegister reg) final;

private:
    bool m_installed = false;

    cp15::Identification m_id;
    cp15::ControlRegister m_ctl;
    cp15::ProtectionUnit m_pu;
    cp15::TCM m_tcm;
    cp15::Cache m_cache;
};

} // namespace armajitto::arm
