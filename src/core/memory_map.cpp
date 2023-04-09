#include "armajitto/core/memory_map.hpp"

#include "memory_map_impl.hpp"

namespace armajitto {

MemoryMap::MemoryMap(size_t pageSize)
    : m_impl(std::make_unique<Impl>(pageSize)) {}

MemoryMap::~MemoryMap() = default;

void MemoryMap::Map(MemoryArea areas, uint8_t layer, uint32_t baseAddress, uint32_t size, MemoryAttributes attrs,
                    uint8_t *ptr, uint64_t mirrorSize) {
    auto &impl = *m_impl;
    auto bmAreas = BitmaskEnum(areas);
    if (bmAreas.AllOf(MemoryArea::CodeRead)) {
        impl.codeRead.Map(layer, baseAddress, size, attrs, ptr, mirrorSize);
    }
    if (bmAreas.AllOf(MemoryArea::DataRead)) {
        impl.dataRead.Map(layer, baseAddress, size, attrs, ptr, mirrorSize);
    }
    if (bmAreas.AllOf(MemoryArea::DataWrite)) {
        impl.dataWrite.Map(layer, baseAddress, size, attrs, ptr, mirrorSize);
    }
}

void MemoryMap::Unmap(MemoryArea areas, uint8_t layer, uint32_t baseAddress, uint64_t size) {
    auto &impl = *m_impl;
    auto bmAreas = BitmaskEnum(areas);
    if (bmAreas.AllOf(MemoryArea::CodeRead)) {
        impl.codeRead.Unmap(layer, baseAddress, size);
    }
    if (bmAreas.AllOf(MemoryArea::DataRead)) {
        impl.dataRead.Unmap(layer, baseAddress, size);
    }
    if (bmAreas.AllOf(MemoryArea::DataWrite)) {
        impl.dataWrite.Unmap(layer, baseAddress, size);
    }
}

} // namespace armajitto
