#include "armajitto/guest/arm/state.hpp"

#include "armajitto/guest/arm/coprocessors/coproc_null.hpp"

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
    CPSR().u32 = 0;
    CPSR().mode = Mode::Supervisor;
    CPSR().i = 1;
    CPSR().f = 1;
    CPSR().t = 0;

    m_irqLine = false;
    m_execState = ExecState::Running;

    m_cp15.Reset();
}

void State::JumpTo(uint32_t address, bool thumb) {
    GPR(GPR::PC) = address + (thumb ? 4 : 8);
    CPSR().t = thumb;
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

uint32_t &State::GPR(enum GPR gpr, Mode mode) {
    const auto index = static_cast<size_t>(gpr) + static_cast<size_t>(mode) * 16;
    assert(static_cast<size_t>(gpr) < 16);
    assert(static_cast<size_t>(mode) < 32);
    return *m_gprPtrs[index];
}

uint32_t &State::GPR(enum GPR gpr) {
    return GPR(gpr, CPSR().mode);
}

PSR &State::CPSR() {
    return m_psrs[0];
}

PSR &State::SPSR(Mode mode) {
    const auto index = static_cast<size_t>(mode);
    assert(index < 32);
    return *m_psrPtrs[index];
}

PSR &State::SPSR() {
    return SPSR(CPSR().mode);
}

bool &State::IRQLine() {
    return m_irqLine;
}

ExecState &State::ExecutionState() {
    return m_execState;
}

Coprocessor &State::GetCoprocessor(uint8_t cpnum) {
    switch (cpnum) {
    case 14: return m_cp14;
    case 15: return m_cp15;
    default: return arm::NullCoprocessor::Instance();
    }
}

DummyDebugCoprocessor &State::GetDummyDebugCoprocessor() {
    return m_cp14;
}

SystemControlCoprocessor &State::GetSystemControlCoprocessor() {
    return m_cp15;
}

const uint32_t &State::GPR(enum GPR gpr, Mode mode) const {
    return const_cast<State *>(this)->GPR(gpr, mode);
}

const uint32_t &State::GPR(enum GPR gpr) const {
    return const_cast<State *>(this)->GPR(gpr);
}

const PSR &State::CPSR() const {
    return const_cast<State *>(this)->CPSR();
}

const PSR &State::SPSR(Mode mode) const {
    return const_cast<State *>(this)->SPSR(mode);
}

const PSR &State::SPSR() const {
    return const_cast<State *>(this)->SPSR();
}

const bool &State::IRQLine() const {
    return const_cast<State *>(this)->IRQLine();
}

const ExecState &State::ExecutionState() const {
    return const_cast<State *>(this)->ExecutionState();
}

const Coprocessor &State::GetCoprocessor(uint8_t cpnum) const {
    return const_cast<State *>(this)->GetCoprocessor(cpnum);
}

const DummyDebugCoprocessor &State::GetDummyDebugCoprocessor() const {
    return const_cast<State *>(this)->GetDummyDebugCoprocessor();
}

const SystemControlCoprocessor &State::GetSystemControlCoprocessor() const {
    return const_cast<State *>(this)->GetSystemControlCoprocessor();
}

} // namespace armajitto::arm
