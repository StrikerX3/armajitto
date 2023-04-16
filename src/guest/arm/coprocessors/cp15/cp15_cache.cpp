#include "armajitto/guest/arm/coprocessors/cp15/cp15_cache.hpp"

#include "util/bit_ops.hpp"

#include <algorithm>
#include <bit>
#include <utility>

namespace armajitto::arm::cp15 {

void Cache::Configure(const Configuration &config) {
    auto adjustSize = [](uint32_t size) -> std::pair<cache::Size, bool> {
        if (size == 0) {
            return {cache::Size::_0KB, true};
        } else if (size <= 4096u) {
            return {cache::Size::_4KB, false};
        } else if (size > 1048576u) {
            return {cache::Size::_1024KB, false};
        }

        const uint32_t nextPow2 = bit::bitceil(size);
        return {static_cast<cache::Size>(std::countr_zero(nextPow2) - 9), false};
    };

    if (config.code.size == 0) {
        params.codeCacheParams = 0;
        params.codeCacheBaseSize = 1;
    } else {
        auto [size, m] = adjustSize(config.code.size);
        params.codeCacheLineLength = config.code.lineLength;
        params.codeCacheBaseSize = m;
        params.codeCacheAssociativity = config.code.associativity;
        params.codeCacheSize = size;
        params._codeCachePadding = 0;
    }

    if (config.data.size == 0) {
        params.dataCacheParams = 0;
        params.dataCacheBaseSize = 1;
    } else {
        auto [size, m] = adjustSize(config.data.size);
        params.dataCacheLineLength = config.data.lineLength;
        params.dataCacheBaseSize = m;
        params.dataCacheAssociativity = config.data.associativity;
        params.dataCacheSize = size;
        params._dataCachePadding = 0;
    }

    params.separateCodeDataCaches = config.separateCodeDataCaches;
    params.type = config.type;
    params._padding = 0;
}

} // namespace armajitto::arm::cp15
