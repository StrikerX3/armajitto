#pragma once

#include "armajitto/util/bit_ops.hpp"

#include <cstdint>
#include <vector>

namespace armajitto::arm::cp15 {

struct TCM {
    std::vector<uint8_t> itcm;
    std::vector<uint8_t> dtcm;

    uint32_t itcmParams;
    uint32_t itcmWriteSize;
    uint32_t itcmReadSize;

    uint32_t dtcmParams;
    uint32_t dtcmBase;
    uint32_t dtcmWriteSize;
    uint32_t dtcmReadSize;

    // -------------------------------------------------------------------------

    struct Configuration {
        uint32_t itcmSize;
        uint32_t dtcmSize;
    };

    void Reset();

    void Configure(const Configuration &params);
    void Disable();
};

} // namespace armajitto::arm::cp15
