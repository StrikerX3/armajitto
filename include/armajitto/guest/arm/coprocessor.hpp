#pragma once

#include "cop_register.hpp"

#include <cstdint>

namespace armajitto::arm {

class Coprocessor {
public:
    virtual bool IsPresent() = 0;

    virtual bool SupportsExtendedRegTransfers() = 0;

    virtual uint32_t LoadRegister(CopRegister reg) = 0;
    virtual void StoreRegister(CopRegister reg, uint32_t value) = 0;
    virtual bool RegStoreHasSideEffects(CopRegister reg) = 0;

    virtual uint32_t LoadExtRegister(CopRegister reg) = 0;
    virtual void StoreExtRegister(CopRegister reg, uint32_t value) = 0;
    virtual bool ExtRegStoreHasSideEffects(CopRegister reg) = 0;
};

} // namespace armajitto::arm
