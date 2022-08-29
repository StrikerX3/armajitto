#pragma once

#include <cstdint>

namespace armajitto::arm::cp15 {

struct ProtectionUnit {
    uint32_t dataCachabilityBits;
    uint32_t codeCachabilityBits;
    uint32_t bufferabilityBits;

    uint32_t dataAccessPermissions;
    uint32_t codeAccessPermissions;

    union Region {
        uint32_t u32;
        struct {
            uint32_t           //
                enable : 1,    // 0     Protection Region Enable (0=Disable, 1=Enable)
                size : 5,      // 1-5   Protection Region Size   (2 SHL X) ;min=(X=11)=4KB, max=(X=31)=4GB
                : 6,           // 6-11  Reserved/zero
                baseAddr : 20; // 12-31 Protection Region Base address (Addr = Y*4K; must be SIZE-aligned)
        };
    };

    Region regions[8];

    void Reset();
};

} // namespace armajitto::arm::cp15
