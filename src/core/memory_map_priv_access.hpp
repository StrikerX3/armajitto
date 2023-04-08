#pragma once

#include "memory_map_impl.hpp"

namespace armajitto {

struct MemoryMapPrivateAccess {
    MemoryMapPrivateAccess(MemoryMap &memMap)
        : codeRead(memMap.m_impl->codeRead)
        , dataRead(memMap.m_impl->dataRead)
        , dataWrite(memMap.m_impl->dataWrite) {}

    util::LayeredMemoryMap<3, MemoryAttributes> &codeRead;
    util::LayeredMemoryMap<3, MemoryAttributes> &dataRead;
    util::LayeredMemoryMap<3, MemoryAttributes> &dataWrite;
};

} // namespace armajitto
