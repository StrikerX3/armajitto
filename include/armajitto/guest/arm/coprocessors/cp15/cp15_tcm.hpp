#pragma once

#include "armajitto/guest/arm/coprocessors/cp15/cp15_defs.hpp"

#include "armajitto/util/bit_ops.hpp"

#include <cstdint>
#include <vector>

namespace armajitto::arm::cp15 {

struct TCM {
    union {
        uint32_t u32;
        struct {
            uint32_t _rsvd0 : 2;
            uint32_t itcmAbsent : 1;
            uint32_t _rsvd3 : 3;
            tcm::Size itcmSize : 4;
            uint32_t _rsvd10 : 4;
            uint32_t dtcmAbsent : 1;
            uint32_t _rsvd15 : 3;
            tcm::Size dtcmSize : 4;
            uint32_t _rsvd22 : 10;
        };
    } params;

    uint32_t itcmParams;
    uint32_t itcmWriteSize;
    uint32_t itcmReadSize;

    uint32_t dtcmParams;
    uint32_t dtcmBase;
    uint32_t dtcmWriteSize;
    uint32_t dtcmReadSize;

    uint8_t *itcm = nullptr;
    uint8_t *dtcm = nullptr;
    uint32_t itcmSize;
    uint32_t dtcmSize;

    // -------------------------------------------------------------------------

    struct Configuration {
        uint32_t itcmSize;
        uint32_t dtcmSize;
    };

    ~TCM();

    void Reset();

    void Configure(const Configuration &params);
    void Disable();

    void SetupITCM(bool enable, bool load);
    void SetupDTCM(bool enable, bool load);
};

} // namespace armajitto::arm::cp15
