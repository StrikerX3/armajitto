#include "armajitto/guest/arm/coprocessors/cp15/cp15_cache.hpp"

#include "util/bit_ops.hpp"

#include <algorithm>
#include <bit>
#include <utility>

namespace armajitto::arm::cp15 {

void Cache::Configure(const Configuration &config) {
    auto adjustSize = [](uint32_t size) -> std::pair<cache::Size, bool> {
        if (size == 0) {
            return {cache::Size::_512BOr768B, true};
        } else if (size <= 512) {
            return {cache::Size::_512BOr768B, false};
        } else if (size > 65536u) {
            return {cache::Size::_64KBOr96KB, true};
        }

        const uint32_t nextPow2 = std::clamp(bit::bitceil(size), 512u, 65536u);
        const uint32_t prevMSize = nextPow2 / 4 * 3;
        if (size <= prevMSize) {
            return {static_cast<cache::Size>(std::countr_zero(prevMSize) - 8), true};
        } else {
            return {static_cast<cache::Size>(std::countr_zero(nextPow2) - 9), false};
        }
    };

    if (config.code.size == 0) {
        params.codeCacheLineLength = cache::LineLength::_8B;
        params.codeCacheM = 1;
        params.codeCacheAssociativity = cache::Associativity::_1WayOrAbsent;
        params.codeCacheSize = cache::Size::_512BOr768B;
        params._codeCachePadding = 0;
    } else {
        auto [size, m] = adjustSize(config.code.size);
        params.codeCacheLineLength = config.code.lineLength;
        params.codeCacheM = m;
        params.codeCacheAssociativity = config.code.associativity;
        params.codeCacheSize = size;
        params._codeCachePadding = 0;
    }

    if (config.data.size == 0) {
        params.dataCacheLineLength = cache::LineLength::_8B;
        params.dataCacheM = 1;
        params.dataCacheAssociativity = cache::Associativity::_1WayOrAbsent;
        params.dataCacheSize = cache::Size::_512BOr768B;
        params._dataCachePadding = 0;
    } else {
        auto [size, m] = adjustSize(config.data.size);
        params.dataCacheLineLength = config.data.lineLength;
        params.dataCacheM = m;
        params.dataCacheAssociativity = config.data.associativity;
        params.dataCacheSize = size;
        params._dataCachePadding = 0;
    }

    params.separateCodeDataCaches = config.separateCodeDataCaches;
    params.type = config.type;
    params._padding = 0;
}

} // namespace armajitto::arm::cp15
