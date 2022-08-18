#pragma once

#include <cstdint>

namespace armajitto::arm {

class Coprocessor {
public:
    virtual bool IsPresent() = 0;

    virtual bool SupportsExtendedRegTransfers() = 0;

    virtual uint32_t LoadRegister(uint8_t opcode1, uint8_t crn, uint8_t crm, uint8_t opcode2) = 0;
    virtual void StoreRegister(uint8_t opcode1, uint8_t crn, uint8_t crm, uint8_t opcode2, uint32_t value) = 0;
    virtual bool RegStoreHasSideEffects(uint8_t opcode1, uint8_t crn, uint8_t crm, uint8_t opcode2) = 0;

    virtual uint32_t LoadExtRegister(uint8_t opcode1, uint8_t crn, uint8_t crm, uint8_t opcode2) = 0;
    virtual void StoreExtRegister(uint8_t opcode1, uint8_t crn, uint8_t crm, uint8_t opcode2, uint32_t value) = 0;
    virtual bool ExtRegStoreHasSideEffects(uint8_t opcode1, uint8_t crn, uint8_t crm, uint8_t opcode2) = 0;
};

} // namespace armajitto::arm
