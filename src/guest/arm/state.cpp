#include "armajitto/guest/arm/state.hpp"

#include "armajitto/util/pointer_cast.hpp"

namespace armajitto::arm {

State::State()
    : m_cp15(m_execState) {
    for (uint32_t mode = 0; mode < 32; mode++) {
        const auto normMode = Normalize(static_cast<Mode>(mode));

        // 0..7: shared by all modes
        for (uint32_t gpr = 0; gpr <= 7; gpr++) {
            uint32_t index = gpr + mode * 16;
            m_gprPtrs[index] = &m_regsUSR[gpr];
        }

        // 8..12: FIQ has banked registers
        for (uint32_t gpr = 8; gpr <= 12; gpr++) {
            uint32_t index = gpr + mode * 16;
            if (normMode == Mode::FIQ) {
                m_gprPtrs[index] = &m_regsFIQ[gpr - 8];
            } else {
                m_gprPtrs[index] = &m_regsUSR[gpr];
            }
        }

        // 13..14: SVC, ABT, IRQ, UND and FIQ have banked registers
        for (uint32_t gpr = 13; gpr <= 14; gpr++) {
            uint32_t index = gpr + mode * 16;
            switch (normMode) {
            case Mode::Supervisor: m_gprPtrs[index] = &m_regsSVC[gpr - 13]; break;
            case Mode::Abort: m_gprPtrs[index] = &m_regsABT[gpr - 13]; break;
            case Mode::IRQ: m_gprPtrs[index] = &m_regsIRQ[gpr - 13]; break;
            case Mode::Undefined: m_gprPtrs[index] = &m_regsUND[gpr - 13]; break;
            case Mode::FIQ: m_gprPtrs[index] = &m_regsFIQ[gpr - 8]; break;
            default: m_gprPtrs[index] = &m_regsUSR[gpr]; break;
            }
        }

        // 15: shared by all modes
        m_gprPtrs[15 + mode * 16] = &m_regsUSR[15];

        const auto modeIndex = NormalizedIndex(static_cast<Mode>(mode));
        m_psrPtrs[mode] = &m_psrs[modeIndex];
    }

    const uintptr_t thisPtr = CastUintPtr(this);

    for (size_t i = 0; i < kNumGPREntries; i++) {
        m_gprOffsets[i] = CastUintPtr(m_gprPtrs[i]) - thisPtr;
    }
    for (size_t i = 0; i < kNumPSREntries; i++) {
        m_psrOffsets[i] = CastUintPtr(m_psrPtrs[i]) - thisPtr;
    }

    m_gprOffsetsOffset = CastUintPtr(m_gprOffsets.data()) - thisPtr;

    m_irqLineOffset = CastUintPtr(&m_irqLine) - thisPtr;
    m_execStateOffset = CastUintPtr(&m_execState) - thisPtr;

    Reset();
}

void State::Reset() {
    m_regsUSR.fill(0);
    m_regsSVC.fill(0);
    m_regsABT.fill(0);
    m_regsIRQ.fill(0);
    m_regsUND.fill(0);
    m_regsFIQ.fill(0);
    CPSR().u32 = 0;
    CPSR().mode = Mode::Supervisor;
    CPSR().i = 1;
    CPSR().f = 1;
    CPSR().t = 0;

    m_irqLine = false;
    m_execState = ExecState::Running;

    m_cp15.Reset();
}

} // namespace armajitto::arm
