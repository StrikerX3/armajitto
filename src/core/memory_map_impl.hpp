#pragma once

#include "armajitto/core/memory_map.hpp"

#include "util/layered_memory_map.hpp"
#include "util/noitree.hpp"

namespace armajitto {

struct MemoryMap::Impl {
    Impl(size_t pageSize)
        : codeRead(pageSize)
        , dataRead(pageSize)
        , dataWrite(pageSize) {}

    util::LayeredMemoryMap<3> codeRead;
    util::LayeredMemoryMap<3> dataRead;
    util::LayeredMemoryMap<3> dataWrite;

    util::NonOverlappingIntervalTree<uint32_t, Attributes> codeReadAttrs;
    util::NonOverlappingIntervalTree<uint32_t, Attributes> dataReadAttrs;
    util::NonOverlappingIntervalTree<uint32_t, Attributes> dataWriteAttrs;
};

} // namespace armajitto
