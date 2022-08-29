#pragma once

#include <cstdint>

namespace armajitto::arm::cp15 {

// CP15 ID code register, post-ARM7 format:
//  31         24 23     20 19          16 15                  4 3        0
// | Implementor | Variant | Architecture | Primary part number | Revision |
union Identification {
    uint32_t u32;
    struct {
        uint32_t revision : 4;
        uint32_t primaryPartNumber : 12;
        uint32_t architecture : 4;
        uint32_t variant : 4;
        uint32_t implementor : 8;
    };
};

} // namespace armajitto::arm::cp15