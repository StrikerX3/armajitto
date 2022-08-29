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
};

} // namespace armajitto::arm
