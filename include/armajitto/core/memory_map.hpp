#pragma once

#include <memory>

namespace armajitto {

struct MemoryMapHostAccess;

struct MemoryMap {
    enum class Areas {
        CodeRead = (1 << 0),
        DataRead = (1 << 1),
        DataWrite = (1 << 2),

        AllRead = CodeRead | DataRead,
        AllData = DataRead | DataWrite,
        All = CodeRead | DataRead | DataWrite,
    };

    MemoryMap(size_t pageSize);
    ~MemoryMap();

    void Map(Areas areas, uint8_t layer, uint32_t baseAddress, uint32_t size, uint8_t *ptr,
             uint64_t mirrorSize = 0x1'0000'0000);

    void Unmap(Areas areas, uint8_t layer, uint32_t baseAddress, uint64_t size);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    friend struct MemoryMapHostAccess;
};

} // namespace armajitto
