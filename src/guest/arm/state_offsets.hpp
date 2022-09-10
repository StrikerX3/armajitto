#pragma once

#include "armajitto/guest/arm/state.hpp"

#include "util/pointer_cast.hpp"

namespace armajitto::arm {

class StateOffsets {
public:
    StateOffsets(State &state) {
        const uintptr_t statePtr = CastUintPtr(&state);

        for (size_t i = 0; i < State::kNumGPREntries; i++) {
            m_gprOffsets[i] = CastUintPtr(state.m_gprPtrs[i]) - statePtr;
        }
        for (size_t i = 0; i < State::kNumPSREntries; i++) {
            m_psrOffsets[i] = CastUintPtr(state.m_psrPtrs[i]) - statePtr;
        }

        m_gprTableOffset = CastUintPtr(state.m_gprPtrs.data()) - statePtr;

        m_irqLineOffset = CastUintPtr(&state.m_irqLine) - statePtr;
        m_execStateOffset = CastUintPtr(&state.m_execState) - statePtr;
    }

    uintptr_t GPROffset(enum GPR gpr, enum Mode mode) const {
        const auto index = static_cast<size_t>(gpr) + static_cast<size_t>(mode) * 16;
        assert(index < State::kNumGPREntries);
        return m_gprOffsets[index];
    }

    uintptr_t GPRTableOffset() const {
        return m_gprTableOffset;
    }

    uintptr_t CPSROffset() const {
        return m_psrOffsets[0];
    }

    uintptr_t SPSROffset(enum Mode mode) const {
        const auto index = static_cast<size_t>(mode);
        assert(index < State::kNumPSREntries);
        return m_psrOffsets[index];
    }

    uintptr_t IRQLineOffset() const {
        return m_irqLineOffset;
    }

    uintptr_t ExecutionStateOffset() const {
        return m_execStateOffset;
    }

private:
    // Lookup tables of GPRs and PSRs offsets
    std::array<uintptr_t, State::kNumGPREntries> m_gprOffsets;
    std::array<uintptr_t, State::kNumPSREntries> m_psrOffsets;
    uintptr_t m_gprTableOffset;

    uintptr_t m_irqLineOffset;
    uintptr_t m_execStateOffset;
};

} // namespace armajitto::arm
