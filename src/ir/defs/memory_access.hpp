#pragma once

namespace armajitto::ir {

enum class MemAccessType { Sequential, Nonsequential };
enum class MemAccessBus { Code, Data };
enum class MemAccessMode { Aligned, Signed, Unaligned };
enum class MemAccessSize { Byte, Half, Word };

} // namespace armajitto::ir
