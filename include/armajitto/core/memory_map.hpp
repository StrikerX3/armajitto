#pragma once

#include "layered_memory_map.hpp"

namespace armajitto {

struct MemoryMap {
    MemoryMap(size_t pageSize)
        : codeRead(pageSize)
        , dataRead(pageSize)
        , dataWrite(pageSize) {}

    LayeredMemoryMap<3> codeRead;
    LayeredMemoryMap<3> dataRead;
    LayeredMemoryMap<3> dataWrite;
};

} // namespace armajitto
