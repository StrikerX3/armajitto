#pragma once

#include "armajitto/guest/arm/coprocessors/coproc_15_sys_control.hpp"

namespace armajitto::arm {

struct SystemControlCoprocessor::PrivateAccess {
    PrivateAccess(SystemControlCoprocessor &cp15)
        : m_cp15(cp15) {}

    // Configures the callback invoked when the code cache is invalidated.
    // Should be automatically invoked by hosts.
    void SetInvalidateCodeCacheCallback(InvalidateCodeCacheCallback callback, void *ctx) {
        m_cp15.m_invalidateCodeCacheCallback = callback;
        m_cp15.m_invalidateCodeCacheCallbackCtx = ctx;
    }

private:
    SystemControlCoprocessor &m_cp15;
};

} // namespace armajitto::arm
