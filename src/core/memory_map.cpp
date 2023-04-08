#include "armajitto/core/memory_map.hpp"

#include "memory_map_impl.hpp"

#include "util/bitmask_enum.hpp"

ENABLE_BITMASK_OPERATORS(armajitto::MemoryMap::Areas);

namespace armajitto {

MemoryMap::MemoryMap(size_t pageSize)
    : m_impl(std::make_unique<Impl>(pageSize)) {}

MemoryMap::~MemoryMap() = default;

void MemoryMap::Map(Areas areas, uint8_t layer, uint32_t baseAddress, uint32_t size, Attributes attrs, uint8_t *ptr,
                    uint64_t mirrorSize) {

    auto bmAreas = BitmaskEnum(areas);
    if (bmAreas.AllOf(Areas::CodeRead)) {
        m_impl->codeRead.Map(layer, baseAddress, size, ptr, mirrorSize);
        m_impl->codeReadAttrs.Insert(baseAddress, baseAddress + size - 1, attrs);
    }
    if (bmAreas.AllOf(Areas::DataRead)) {
        m_impl->dataRead.Map(layer, baseAddress, size, ptr, mirrorSize);
        m_impl->dataReadAttrs.Insert(baseAddress, baseAddress + size - 1, attrs);
    }
    if (bmAreas.AllOf(Areas::DataWrite)) {
        m_impl->dataWrite.Map(layer, baseAddress, size, ptr, mirrorSize);
        m_impl->dataWriteAttrs.Insert(baseAddress, baseAddress + size - 1, attrs);
    }
}

void MemoryMap::Unmap(Areas areas, uint8_t layer, uint32_t baseAddress, uint64_t size) {
    auto bmAreas = BitmaskEnum(areas);
    if (bmAreas.AllOf(Areas::CodeRead)) {
        m_impl->codeRead.Unmap(layer, baseAddress, size);
        m_impl->codeReadAttrs.Remove(baseAddress, baseAddress + size - 1);
    }
    if (bmAreas.AllOf(Areas::DataRead)) {
        m_impl->dataRead.Unmap(layer, baseAddress, size);
        m_impl->dataReadAttrs.Remove(baseAddress, baseAddress + size - 1);
    }
    if (bmAreas.AllOf(Areas::DataWrite)) {
        m_impl->dataWrite.Unmap(layer, baseAddress, size);
        m_impl->dataWriteAttrs.Remove(baseAddress, baseAddress + size - 1);
    }
}

} // namespace armajitto
