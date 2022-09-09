#pragma once

#include "coprocessor.hpp"
#include "exceptions.hpp"
#include "exec_state.hpp"
#include "gpr.hpp"
#include "mode.hpp"
#include "psr.hpp"

#include "coprocessors/coproc_14_debug_dummy.hpp"
#include "coprocessors/coproc_15_sys_control.hpp"
#include "coprocessors/coproc_null.hpp"

#include <array>
#include <cassert>
#include <cstdint>

namespace armajitto::arm {

class State {
public:
    State();

    void Reset();

    // Convenience method that sets PC and the CPSR T bit to the specified values.
    // Also applies the pipeline offset to the address (+8 for ARM, +4 for Thumb).
    void JumpTo(uint32_t address, bool thumb) {
        GPR(GPR::PC) = address + (thumb ? 4 : 8);
        CPSR().t = thumb;
    }

    // Convenience method to switch to the specified mode, automatically storing SPSR if necessary.
    void SetMode(Mode mode) {
        auto spsrIndex = NormalizedIndex(mode);
        if (spsrIndex > 0) {
            SPSR(mode) = CPSR();
        }
        CPSR().mode = mode;
    }

    // Convenience method that forces the processor to enter the specified exception vector.
    void EnterException(Exception vector) {
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

    uint32_t &GPR(GPR gpr, Mode mode) {
        const auto index = static_cast<size_t>(gpr) + static_cast<size_t>(mode) * 16;
        assert(static_cast<size_t>(gpr) < 16);
        assert(static_cast<size_t>(mode) < 32);
        return *m_gprPtrs[index];
    }

    const uint32_t &GPR(enum GPR gpr, Mode mode) const {
        return const_cast<State *>(this)->GPR(gpr, mode);
    }

    uint32_t &GPR(enum GPR gpr) {
        return GPR(gpr, CPSR().mode);
    }

    const uint32_t &GPR(enum GPR gpr) const {
        return const_cast<State *>(this)->GPR(gpr);
    }

    PSR &CPSR() {
        return m_psrs[0];
    }

    const PSR &CPSR() const {
        return const_cast<State *>(this)->CPSR();
    }

    PSR &SPSR(Mode mode) {
        const auto index = static_cast<size_t>(mode);
        assert(index < 32);
        return *m_psrPtrs[index];
    }

    const PSR &SPSR(Mode mode) const {
        return const_cast<State *>(this)->SPSR(mode);
    }

    PSR &SPSR() {
        return SPSR(CPSR().mode);
    }

    const PSR &SPSR() const {
        return const_cast<State *>(this)->SPSR();
    }

    bool &IRQLine() {
        return m_irqLine;
    }

    const bool &IRQLine() const {
        return const_cast<State *>(this)->IRQLine();
    }

    ExecState &ExecutionState() {
        return m_execState;
    }

    const ExecState &ExecutionState() const {
        return const_cast<State *>(this)->ExecutionState();
    }

    Coprocessor &GetCoprocessor(uint8_t cpnum) {
        switch (cpnum) {
        case 14: return m_cp14;
        case 15: return m_cp15;
        default: return arm::NullCoprocessor::Instance();
        }
    }

    DummyDebugCoprocessor &GetDummyDebugCoprocessor() {
        return m_cp14;
    }

    SystemControlCoprocessor &GetSystemControlCoprocessor() {
        return m_cp15;
    }

    // -------------------------------------------------------------------------
    // Helper methods for the host compiler

    uintptr_t GPROffset(enum GPR gpr, enum Mode mode) const {
        const auto index = static_cast<size_t>(gpr) + static_cast<size_t>(mode) * 16;
        assert(index < kNumGPREntries);
        return m_gprOffsets[index];
    }

    uintptr_t GPROffsetsOffset() const {
        return m_gprOffsetsOffset;
    }

    uintptr_t CPSROffset() const {
        return m_psrOffsets[0];
    }

    uintptr_t SPSROffset(enum Mode mode) const {
        const auto index = static_cast<size_t>(mode);
        assert(index < kNumPSREntries);
        return m_psrOffsets[index];
    }

    uintptr_t IRQLineOffset() const {
        return m_irqLineOffset;
    }

    uintptr_t ExecutionStateOffset() const {
        return m_execStateOffset;
    }

private:
    // ARM registers per mode (abridged)
    //
    //   User      System    Supervis. Abort     Undefined IRQ       Fast IRQ
    //   R0        R0        R0        R0        R0        R0        R0
    //   ...       ...       ...       ...       ...       ...       ...
    //   R7        R7        R7        R7        R7        R7        R7
    //   R8        R8        R8        R8        R8        R8        R8_fiq
    //   ...       ...       ...       ...       ...       ...       ...
    //   R12       R12       R12       R12       R12       R12       R12_fiq
    //   R13       R13       R13_svc   R13_abt   R13_und   R13_irq   R13_fiq
    //   R14       R14       R14_svc   R14_abt   R14_und   R14_irq   R14_fiq
    //   R15       R15       R15       R15       R15       R15       R15
    //
    //   CPSR      CPSR      CPSR      CPSR      CPSR      CPSR      CPSR
    //   -         -         SPSR_svc  SPSR_abt  SPSR_und  SPSR_irq  SPSR_fiq

    // General purpose registers
    // R0..R7 and R15 for all modes
    // R8..R12 for all modes but SVC, ABT, IRQ, UND and FIQ
    // R13 and R14 for all modes but FIQ
    alignas(16) std::array<uint32_t, 16> m_regsUSR;
    std::array<uint32_t, 14 - 13 + 1> m_regsSVC; // R13 and R14 for SVC
    std::array<uint32_t, 14 - 13 + 1> m_regsABT; // R13 and R14 for ABT
    std::array<uint32_t, 14 - 13 + 1> m_regsIRQ; // R13 and R14 for IRQ
    std::array<uint32_t, 14 - 13 + 1> m_regsUND; // R13 and R14 for UND
    std::array<uint32_t, 14 - 8 + 1> m_regsFIQ;  // R8..R14 for FIQ

    // PSR per mode -- NormalizedIndex(Mode)
    // [0] CPSR
    // [1] SPSR_fiq
    // [2] SPSR_irq
    // [3] SPSR_svc
    // [4] SPSR_abt
    // [5] SPSR_und
    std::array<union PSR, kNumBankedModes> m_psrs;

    // Coprocessors
    DummyDebugCoprocessor m_cp14;
    SystemControlCoprocessor m_cp15;

    // IRQ line
    bool m_irqLine = false;

    // Execution state.
    // When halted or stopped, the CPU stops executing code until the IRQ line is raised.
    ExecState m_execState = ExecState::Running;

    // -------------------------------------------------------------------------
    // Lookup tables

    static constexpr size_t kNumGPREntries = 16 * 32;
    static constexpr size_t kNumPSREntries = 32;

    // Lookup tables of GPRs and PSRs per mode (full range, for fast access)
    std::array<uint32_t *, kNumGPREntries> m_gprPtrs;
    std::array<union PSR *, kNumPSREntries> m_psrPtrs;

    // Lookup tables of GPRs and PSRs offsets
    std::array<uintptr_t, kNumGPREntries> m_gprOffsets;
    std::array<uintptr_t, kNumPSREntries> m_psrOffsets;
    uintptr_t m_gprOffsetsOffset;

    uintptr_t m_irqLineOffset;
    uintptr_t m_execStateOffset;
};

} // namespace armajitto::arm
