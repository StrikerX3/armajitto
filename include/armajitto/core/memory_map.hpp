#pragma once

#include "memory_params.hpp"

#include <memory>

namespace armajitto {

struct MemoryMapPrivateAccess;

struct MemoryMap {
    MemoryMap(size_t pageSize);
    ~MemoryMap();

    void Map(MemoryArea areas, uint8_t layer, uint32_t baseAddress, uint32_t size, MemoryAttributes attrs, uint8_t *ptr,
             uint64_t mirrorSize = 0x1'0000'0000);

    void Unmap(MemoryArea areas, uint8_t layer, uint32_t baseAddress, uint64_t size);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    friend struct MemoryMapPrivateAccess;
};

} // namespace armajitto
