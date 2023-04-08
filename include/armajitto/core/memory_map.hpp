#pragma once

#include <memory>

namespace armajitto {

struct MemoryMapPrivateAccess;

struct MemoryMap {
    enum class Areas {
        None = 0,

        CodeRead = (1 << 0),
        DataRead = (1 << 1),
        DataWrite = (1 << 2),

        AllRead = CodeRead | DataRead,
        AllData = DataRead | DataWrite,
        All = CodeRead | DataRead | DataWrite,
    };

    enum class Attributes {
        None = 0,

        Readable = (1 << 0),
        Writable = (1 << 1),
        Executable = (1 << 2),

        Constant = (1 << 3),
        Volatile = (1 << 4),
        Dynamic = (1 << 5),

        RW = Readable | Writable,
        RX = Readable | Executable,
        RWX = Readable | Writable | Executable,
    };

    MemoryMap(size_t pageSize);
    ~MemoryMap();

    void Map(Areas areas, uint8_t layer, uint32_t baseAddress, uint32_t size, Attributes attrs, uint8_t *ptr,
             uint64_t mirrorSize = 0x1'0000'0000);

    void Unmap(Areas areas, uint8_t layer, uint32_t baseAddress, uint64_t size);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    friend struct MemoryMapPrivateAccess;
};

} // namespace armajitto
