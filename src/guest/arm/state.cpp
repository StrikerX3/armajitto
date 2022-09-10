#include "armajitto/guest/arm/state.hpp"

#include "guest/arm/exception_vectors.hpp"
#include "guest/arm/mode_utils.hpp"

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

    Reset();
}

void State::Reset() {
    m_regsUSR.fill(0);
    m_regsSVC.fill(0);
    m_regsABT.fill(0);
    m_regsIRQ.fill(0);
    m_regsUND.fill(0);
    m_regsFIQ.fill(0);
    m_psrs.fill({.u32 = 0});

    m_cp15.Reset();

    GPR(GPR::PC) = m_cp15.IsPresent() ? m_cp15.GetControlRegister().baseVectorAddress : 0;

    CPSR().u32 = 0;
    CPSR().mode = Mode::Supervisor;
    CPSR().i = 1;
    CPSR().f = 1;
    CPSR().t = 0;

    m_irqLine = false;
    m_execState = ExecState::Running;
}

void State::SetMode(Mode mode) {
    auto spsrIndex = NormalizedIndex(mode);
    if (spsrIndex > 0) {
        SPSR(mode) = CPSR();
    }
    CPSR().mode = mode;
}

void State::EnterException(Exception vector) {
    const auto &vectorInfo = arm::kExceptionVectorInfos[static_cast<size_t>(vector)];

    const bool t = CPSR().t;
    const uint32_t instrSize = t ? sizeof(uint16_t) : sizeof(uint32_t);
    const uint32_t nn = t ? vectorInfo.thumbOffset : vectorInfo.armOffset;
    const uint32_t pc = GPR(GPR::PC) - instrSize * 2;

    SetMode(vectorInfo.mode);
    CPSR().t = 0;
    CPSR().i = 1;
    if (vectorInfo.F) {
        CPSR().f = 1;
    }

    const uint32_t baseVectorAddress =
        (m_cp15.IsPresent() ? m_cp15.GetControlRegister().baseVectorAddress : 0x00000000);

    GPR(GPR::LR) = pc + nn;
    GPR(GPR::PC) = baseVectorAddress + static_cast<uint32_t>(vector) * 4 + instrSize * 2;
}

} // namespace armajitto::arm
