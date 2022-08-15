#pragma once

#include <cstdint>

namespace armajitto {

class ISystem {
public:
    virtual ~ISystem() = default;

    virtual uint8_t MemReadByte(uint32_t address) = 0;
    virtual uint16_t MemReadHalf(uint32_t address) = 0;
    virtual uint32_t MemReadWord(uint32_t address) = 0;

    virtual void MemWriteByte(uint32_t address, uint8_t value) = 0;
    virtual void MemWriteHalf(uint32_t address, uint16_t value) = 0;
    virtual void MemWriteWord(uint32_t address, uint32_t value) = 0;
};

} // namespace armajitto
