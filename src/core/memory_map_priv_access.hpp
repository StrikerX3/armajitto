#pragma once

#include "memory_map_impl.hpp"

namespace armajitto {

struct MemoryMapPrivateAccess {
    MemoryMapPrivateAccess(MemoryMap &memMap)
        : codeRead(memMap.m_impl->codeRead)
        , dataRead(memMap.m_impl->dataRead)
        , dataWrite(memMap.m_impl->dataWrite)

        , codeReadAttrs(memMap.m_impl->codeReadAttrs)
        , dataReadAttrs(memMap.m_impl->dataReadAttrs)
        , dataWriteAttrs(memMap.m_impl->dataWriteAttrs) {}

    util::LayeredMemoryMap<3> &codeRead;
    util::LayeredMemoryMap<3> &dataRead;
    util::LayeredMemoryMap<3> &dataWrite;

    util::NonOverlappingIntervalTree<uint32_t, MemoryMap::Attributes> &codeReadAttrs;
    util::NonOverlappingIntervalTree<uint32_t, MemoryMap::Attributes> &dataReadAttrs;
    util::NonOverlappingIntervalTree<uint32_t, MemoryMap::Attributes> &dataWriteAttrs;
};

} // namespace armajitto
