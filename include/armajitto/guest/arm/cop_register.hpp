#pragma once

#include <cstdint>

namespace armajitto::arm {

union CopRegister {
    uint16_t u16;
    struct {
        uint16_t opcode2 : 4; // actually 3 bits, but rounded up to 4 to make the u16 field more convenient to use
        uint16_t crm : 4;
        uint16_t crn : 4;
        uint16_t opcode1 : 4; // actually 3 bits, but rounded up to 4 to make the u16 field more convenient to use
    };

    CopRegister()
        : u16(0) {}

    CopRegister(uint16_t u16)
        : u16(u16) {}

    CopRegister(uint8_t opcode1, uint8_t crn, uint8_t crm, uint8_t opcode2)
        : opcode2(opcode2 & 7)
        , crm(crm)
        , crn(crn)
        , opcode1(opcode1 & 7) {}
};

} // namespace armajitto::arm
