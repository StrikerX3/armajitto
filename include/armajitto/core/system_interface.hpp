#pragma once

#include <cstdint>

namespace armajitto {

class ISystem {
public:
    virtual ~ISystem() = default;

    virtual uint8_t ReadByte(uint32_t address) = 0;
    virtual uint16_t ReadHalf(uint32_t address) = 0;
    virtual uint32_t ReadWord(uint32_t address) = 0;

    virtual void WriteByte(uint32_t address, uint8_t value) = 0;
    virtual void WriteHalf(uint32_t address, uint16_t value) = 0;
    virtual void WriteWord(uint32_t address, uint32_t value) = 0;
};

} // namespace armajitto
