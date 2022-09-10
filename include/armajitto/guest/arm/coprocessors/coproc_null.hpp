#pragma once

#include "armajitto/guest/arm/coprocessor.hpp"

namespace armajitto::arm {

class NullCoprocessor : public Coprocessor {
public:
    static NullCoprocessor &Instance() {
        return s_instance;
    }

    bool IsPresent() const final {
        return false;
    }

    uint32_t LoadRegister(CopRegister reg) final {
        return 0;
    }

    void StoreRegister(CopRegister reg, uint32_t value) final {}

    bool RegStoreHasSideEffects(CopRegister reg) const final {
        return false;
    }

private:
    NullCoprocessor() = default;

    static NullCoprocessor s_instance;
};

inline NullCoprocessor NullCoprocessor::s_instance;

} // namespace armajitto::arm
