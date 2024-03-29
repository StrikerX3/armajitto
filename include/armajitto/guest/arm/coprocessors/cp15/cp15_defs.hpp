#pragma once

#include <cstdint>

namespace armajitto::arm::cp15 {

namespace id {
    // Implementors, bits 31..24.
    enum class Implementor : uint32_t {
        ARM = 0x41,   // 'A'
        DEC = 0x44,   // 'D'
        Intel = 0x69, // 'i'
    };

    // Architectures, bits 19..16.
    enum class Architecture : uint32_t {
        v4 = 0x1,
        v4T = 0x2,
        v5 = 0x3,
        v5T = 0x4,
        v5TE = 0x5,
    };

    // Primary part numbers, bits 15..4.
    inline constexpr uint32_t kPrimaryPartNumberARM946 = 0x946;
} // namespace id

// -----------------------------------------------------------------------------

namespace tcm {
    // TCM sizes, bits 21..18 (DTCM) and 9..6 (ITCM)
    enum class Size : uint32_t {
        _0KB = 0b0000,
        _4KB = 0b0011,
        _8KB = 0b0100,
        _16KB = 0b0101,
        _32KB = 0b0110,
        _64KB = 0b0111,
        _128KB = 0b1000,
        _256KB = 0b1001,
        _512KB = 0b1010,
        _1024KB = 0b1011,
    };
} // namespace tcm

// -----------------------------------------------------------------------------

// NOTE: Cache parameters as defined by the ARM946E-S Technical Reference Manual
namespace cache {
    // Cache types, bits 28..25.
    enum class Type : uint32_t {
        // Write-through, cleaning not needed, lockdown not supported
        WriteThrough = 0b0000,

        // Write-through, clean on read, lockdown not supported
        WriteBackReadClean = 0b0001,

        // Write-through, clean on register 7 operations, lockdown not supported
        WriteBackReg7Clean = 0b0010,

        // Write-through, clean on register 7 operations, lockdown supported (format A)
        WriteBackReg7CleanLockdownA = 0b0110,

        // Write-through, clean on register 7 operations, lockdown supported (format B)
        WriteBackReg7CleanLockdownB = 0b0111,
    };

    // Cache sizes, bits 21..18 (data cache) and 9..6 (instruction cache).
    enum class Size : uint32_t {
        _0KB = 0b0000,
        // _1KB = 0b0001,
        // _2KB = 0b0010,
        _4KB = 0b0011,
        _8KB = 0b0100,
        _16KB = 0b0101,
        _32KB = 0b0110,
        _64KB = 0b0111,
        _128KB = 0b1000,
        _256KB = 0b1001,
        _512KB = 0b1010,
        _1024KB = 0b1011,
    };

    // Cache line lengths, bits 13..12 (data cache) and 1..0 (instruction cache).
    enum class LineLength : uint32_t {
        _8B = 0b00,
        _16B = 0b01,
        _32B = 0b10,
        _64B = 0b11,
    };

    // Cache associativities, bits 20..18 (data cache) and 8..6 (instruction cache).
    enum class Associativity : uint32_t {
        _1Way = 0b000, // aka direct mapped
        // _2Way = 0b001,
        _4Way = 0b010,
        // _8Way = 0b011,
        // _16Way = 0b100,
        // _32Way = 0b101,
        // _64Way = 0b110,
        // _128Way = 0b111,
    };

} // namespace cache

} // namespace armajitto::arm::cp15
