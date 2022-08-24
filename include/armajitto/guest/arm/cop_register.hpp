#pragma once

#include <cstdint>

namespace armajitto::arm {

struct CopRegister {
    uint8_t opcode1;
    uint8_t crn;
    uint8_t crm;
    uint8_t opcode2;
};

} // namespace armajitto::arm
