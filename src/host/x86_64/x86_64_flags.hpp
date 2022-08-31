#pragma once

#include <cstdint>

namespace armajitto::x86_64 {

// From Dynarmic:

// This is a constant used to create the x64 flags format from the ARM format.
// NZCV * multiplier: NZCV0NZCV000NZCV
// x64_flags format:  NZ-----C-------V
constexpr uint32_t ARMTox64FlagsMult = 0x1081;

// This is a constant used to create the ARM format from the x64 flags format.
constexpr uint32_t x64ToARMFlagsMult = 0x1021'0000;

constexpr uint32_t ARMflgIPos = 7u;
constexpr uint32_t ARMflgTPos = 5u;
constexpr uint32_t ARMflgQPos = 27u;
constexpr uint32_t ARMflgNZCVShift = 28u;

constexpr uint32_t x64flgIPos = 16u;
constexpr uint32_t x64flgNPos = 15u;
constexpr uint32_t x64flgZPos = 14u;
constexpr uint32_t x64flgCPos = 8u;
constexpr uint32_t x64flgVPos = 0u;

constexpr uint32_t x64flgI = (1u << x64flgIPos);
constexpr uint32_t x64flgN = (1u << x64flgNPos);
constexpr uint32_t x64flgZ = (1u << x64flgZPos);
constexpr uint32_t x64flgC = (1u << x64flgCPos);
constexpr uint32_t x64flgV = (1u << x64flgVPos);

constexpr uint32_t x64FlagsMask = x64flgN | x64flgZ | x64flgC | x64flgV;

} // namespace armajitto::x86_64
