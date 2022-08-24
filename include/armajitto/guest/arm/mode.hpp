#pragma once

#include <array>
#include <cstdint>
#include <format>
#include <string>

namespace armajitto::arm {

enum class Mode : uint32_t {
    User = 0x10,
    FIQ = 0x11,
    IRQ = 0x12,
    Supervisor = 0x13, // aka SWI
    Abort = 0x17,
    Undefined = 0x1B,
    System = 0x1F
};

inline std::string ToString(Mode mode) {
    switch (mode) {
    case Mode::User: return "usr";
    case Mode::FIQ: return "fiq";
    case Mode::IRQ: return "irq";
    case Mode::Supervisor: return "svc";
    case Mode::Abort: return "abt";
    case Mode::Undefined: return "und";
    case Mode::System: return "sys";
    default: return std::format("unk{:x}", static_cast<uint32_t>(mode));
    }
}

// Returns a normalized index for register and PSR banking, for use in arrays.
// There are six banks in total, indexed as follows:
// [0] User, System and all invalid modes
// [1] FIQ
// [2] IRQ
// [3] Supervisor
// [4] Abort
// [5] Undefined
inline size_t NormalizedIndex(Mode mode) {
    static constexpr auto indices = [] {
        std::array<size_t, 32> indices{};
        indices.fill(0);
        indices[static_cast<size_t>(Mode::User)] = 0;
        indices[static_cast<size_t>(Mode::FIQ)] = 1;
        indices[static_cast<size_t>(Mode::IRQ)] = 2;
        indices[static_cast<size_t>(Mode::Supervisor)] = 3;
        indices[static_cast<size_t>(Mode::Abort)] = 4;
        indices[static_cast<size_t>(Mode::Undefined)] = 5;
        indices[static_cast<size_t>(Mode::System)] = 0;
        return indices;
    }();
    return indices[static_cast<size_t>(mode)];
}

inline constexpr size_t kNumNormalizedModeIndices = 6;

} // namespace armajitto::arm
