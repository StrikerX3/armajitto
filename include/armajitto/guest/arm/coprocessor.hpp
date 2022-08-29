#pragma once

#include "cop_register.hpp"

#include <cstdint>

namespace armajitto::arm {

class Coprocessor {
public:
    virtual bool IsPresent() = 0;

    virtual bool SupportsExtendedRegTransfers() {
        return false;
    }

    virtual uint32_t LoadRegister(CopRegister reg) = 0;
    virtual void StoreRegister(CopRegister reg, uint32_t value) = 0;
    virtual bool RegStoreHasSideEffects(CopRegister reg) = 0;

    virtual uint32_t LoadExtRegister(CopRegister reg) {
        return 0;
    }
    virtual void StoreExtRegister(CopRegister reg, uint32_t value) {}
    virtual bool ExtRegStoreHasSideEffects(CopRegister reg) {
        return false;
    }
};

} // namespace armajitto::arm
