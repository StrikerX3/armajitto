#pragma once

#include "armajitto/guest/arm/coprocessors/cp15/cp15_defs.hpp"

#include <cstdint>

namespace armajitto::arm::cp15 {

// NOTE: Cache parameters as defined by the ARM946E-S Technical Reference Manual
struct Cache {
    union {
        uint32_t u32;
        struct {
            cache::LineLength codeCacheLineLength : 2;
            uint32_t codeCacheBaseSize : 1; // 0 = present, 1 = absent
            cache::Associativity codeCacheAssociativity : 3;
            cache::Size codeCacheSize : 4;
            uint32_t _codeCachePadding : 2;

            cache::LineLength dataCacheLineLength : 2;
            uint32_t dataCacheBaseSize : 1; // 0 = present, 1 = absent
            cache::Associativity dataCacheAssociativity : 3;
            cache::Size dataCacheSize : 4;
            uint32_t _dataCachePadding : 2;

            uint32_t separateCodeDataCaches : 1;
            cache::Type type : 4;
            uint32_t _padding : 3;
        };
        struct {
            uint32_t codeCacheParams : 12;
            uint32_t dataCacheParams : 12;
        };
    } params;

    // -------------------------------------------------------------------------

    struct Configuration {
        cache::Type type;
        bool separateCodeDataCaches;
        struct {
            uint32_t size; // 0 = disabled
            cache::LineLength lineLength;
            cache::Associativity associativity;
        } code, data;
    };

    void Configure(const Configuration &config);
};

} // namespace armajitto::arm::cp15
