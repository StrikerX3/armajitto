#include "armajitto/core/memory_map.hpp"

#include "memory_map_impl.hpp"

#include "util/bitmask_enum.hpp"

ENABLE_BITMASK_OPERATORS(armajitto::MemoryMap::Areas);

namespace armajitto {

MemoryMap::MemoryMap(size_t pageSize)
    : m_impl(std::make_unique<Impl>(pageSize)) {}

MemoryMap::~MemoryMap() = default;

void MemoryMap::Map(Areas areas, uint8_t layer, uint32_t baseAddress, uint32_t size, uint8_t *ptr,
                    uint64_t mirrorSize) {

    auto bmAreas = BitmaskEnum(areas);
    if (bmAreas.AllOf(Areas::CodeRead)) {
        m_impl->codeRead.Map(layer, baseAddress, size, ptr, mirrorSize);
    }
    if (bmAreas.AllOf(Areas::DataRead)) {
        m_impl->dataRead.Map(layer, baseAddress, size, ptr, mirrorSize);
    }
    if (bmAreas.AllOf(Areas::DataWrite)) {
        m_impl->dataWrite.Map(layer, baseAddress, size, ptr, mirrorSize);
    }
}

void MemoryMap::Unmap(Areas areas, uint8_t layer, uint32_t baseAddress, uint64_t size) {
    auto bmAreas = BitmaskEnum(areas);
    if (bmAreas.AllOf(Areas::CodeRead)) {
        m_impl->codeRead.Unmap(layer, baseAddress, size);
    }
    if (bmAreas.AllOf(Areas::DataRead)) {
        m_impl->dataRead.Unmap(layer, baseAddress, size);
    }
    if (bmAreas.AllOf(Areas::DataWrite)) {
        m_impl->dataWrite.Unmap(layer, baseAddress, size);
    }
}

} // namespace armajitto
