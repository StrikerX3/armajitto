#pragma once

#include <memory>

namespace armajitto {

enum class MemoryArea {
    None = 0,

    CodeRead = (1 << 0),
    DataRead = (1 << 1),
    DataWrite = (1 << 2),

    AllRead = CodeRead | DataRead,
    AllData = DataRead | DataWrite,
    All = CodeRead | DataRead | DataWrite,
};

enum class MemoryAttributes {
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

} // namespace armajitto
