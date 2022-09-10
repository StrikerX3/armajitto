#pragma once

#include "memory_map_impl.hpp"

namespace armajitto {

struct MemoryMapHostAccess {
    MemoryMapHostAccess(MemoryMap &memMap)
        : codeRead(memMap.m_impl->codeRead)
        , dataRead(memMap.m_impl->dataRead)
        , dataWrite(memMap.m_impl->dataWrite) {}

    util::LayeredMemoryMap<3> &codeRead;
    util::LayeredMemoryMap<3> &dataRead;
    util::LayeredMemoryMap<3> &dataWrite;
};

} // namespace armajitto
