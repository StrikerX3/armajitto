#pragma once

#include "armajitto/guest/arm/mode.hpp"

#include <array>

namespace armajitto::arm {

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

inline Mode Normalize(Mode mode) {
    static constexpr auto modes = [] {
        std::array<Mode, 32> modes{};
        modes.fill(Mode::User);
        modes[static_cast<size_t>(Mode::User)] = Mode::User;
        modes[static_cast<size_t>(Mode::FIQ)] = Mode::FIQ;
        modes[static_cast<size_t>(Mode::IRQ)] = Mode::IRQ;
        modes[static_cast<size_t>(Mode::Supervisor)] = Mode::Supervisor;
        modes[static_cast<size_t>(Mode::Abort)] = Mode::Abort;
        modes[static_cast<size_t>(Mode::Undefined)] = Mode::Undefined;
        modes[static_cast<size_t>(Mode::System)] = Mode::System;
        return modes;
    }();
    return modes[static_cast<size_t>(mode)];
}

inline constexpr size_t kNumBankedModes = 6;

} // namespace armajitto::arm
