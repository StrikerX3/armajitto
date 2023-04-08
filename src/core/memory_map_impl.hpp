#pragma once

#include "armajitto/core/memory_map.hpp"

#include "util/bitmask_enum.hpp"
#include "util/layered_memory_map.hpp"

ENABLE_BITMASK_OPERATORS(armajitto::MemoryArea);
ENABLE_BITMASK_OPERATORS(armajitto::MemoryAttributes);

namespace armajitto {

struct MemoryMap::Impl {
    Impl(size_t pageSize)
        : codeRead(pageSize)
        , dataRead(pageSize)
        , dataWrite(pageSize) {}

    util::LayeredMemoryMap<3, MemoryAttributes> codeRead;
    util::LayeredMemoryMap<3, MemoryAttributes> dataRead;
    util::LayeredMemoryMap<3, MemoryAttributes> dataWrite;
};

} // namespace armajitto
