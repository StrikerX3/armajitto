#pragma once

#include "armajitto/guest/arm/coprocessors/cp15/cp15_defs.hpp"

#include <cstdint>

namespace armajitto::arm::cp15 {

struct Cache {
    union {
        uint32_t u32;
        struct {
            cache::LineLength codeCacheLineLength : 2;
            uint32_t codeCacheM : 1;
            cache::Associativity codeCacheAssociativity : 3;
            cache::Size codeCacheSize : 3;
            uint32_t _codeCachePadding : 3;

            cache::LineLength dataCacheLineLength : 2;
            uint32_t dataCacheM : 1;
            cache::Associativity dataCacheAssociativity : 3;
            cache::Size dataCacheSize : 3;
            uint32_t _dataCachePadding : 3;

            uint32_t separateCodeDataCaches : 1;
            cache::Type type : 4;
            uint32_t _padding : 3;
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
