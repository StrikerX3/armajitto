#pragma once

#include "armajitto/core/memory_map.hpp"

#include "util/layered_memory_map.hpp"

namespace armajitto {

struct MemoryMap::Impl {
    Impl(size_t pageSize)
        : codeRead(pageSize)
        , dataRead(pageSize)
        , dataWrite(pageSize) {}

    util::LayeredMemoryMap<3> codeRead;
    util::LayeredMemoryMap<3> dataRead;
    util::LayeredMemoryMap<3> dataWrite;
};

} // namespace armajitto
