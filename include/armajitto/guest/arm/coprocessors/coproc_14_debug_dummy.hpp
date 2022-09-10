#pragma once

#include "armajitto/guest/arm/coprocessor.hpp"

namespace armajitto::arm {

class DummyDebugCoprocessor : public Coprocessor {
public:
    DummyDebugCoprocessor() = default;

    void Install() {
        m_installed = true;
    }

    void Uninstall() {
        m_installed = false;
    }

    // -------------------------------------------------------------------------
    // Coprocessor interface implementation

    bool IsPresent() const final {
        return m_installed;
    }

    uint32_t LoadRegister(CopRegister reg) final {
        // TODO: return last fetched opcode
        return 0;
    }

    void StoreRegister(CopRegister reg, uint32_t value) final {}

    bool RegStoreHasSideEffects(CopRegister reg) const final {
        return false;
    }

private:
    bool m_installed = false;
};

} // namespace armajitto::arm
