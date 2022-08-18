#pragma once

#include "armajitto/guest/arm/coprocessor.hpp"

namespace armajitto::arm {

class NullCoprocessor : public Coprocessor {
public:
    static NullCoprocessor &Instance() {
        return s_instance;
    }

    bool IsPresent() final {
        return false;
    }

    bool SupportsExtendedRegTransfers() final {
        return false;
    }

    uint32_t LoadRegister(uint8_t opcode1, uint8_t crn, uint8_t crm, uint8_t opcode2) final {
        return 0;
    }

    void StoreRegister(uint8_t opcode1, uint8_t crn, uint8_t crm, uint8_t opcode2, uint32_t value) final {}

    bool RegStoreHasSideEffects(uint8_t opcode1, uint8_t crn, uint8_t crm, uint8_t opcode2) final {
        return false;
    }

    uint32_t LoadExtRegister(uint8_t opcode1, uint8_t crn, uint8_t crm, uint8_t opcode2) final {
        return 0;
    }

    void StoreExtRegister(uint8_t opcode1, uint8_t crn, uint8_t crm, uint8_t opcode2, uint32_t value) final {}

    bool ExtRegStoreHasSideEffects(uint8_t opcode1, uint8_t crn, uint8_t crm, uint8_t opcode2) final {
        return false;
    }

private:
    NullCoprocessor() = default;

    static NullCoprocessor s_instance;
};

inline NullCoprocessor NullCoprocessor::s_instance;

} // namespace armajitto::arm
