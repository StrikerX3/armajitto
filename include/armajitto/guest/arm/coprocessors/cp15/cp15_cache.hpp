#pragma once

#include "armajitto/guest/arm/coprocessors/cp15/cp15_defs.hpp"

#include <cstdint>

namespace armajitto::arm::cp15 {

struct Cache {

    // -------------------------------------------------------------------------

    struct Configuration {
        cache::Type type;
        struct {
            uint32_t size; // 0 = disabled
            cache::LineLength lineLength;
            cache::Associativity associativity;
        } code, data;
    };

    void Configure(const Configuration &config);
};

} // namespace armajitto::arm::cp15
