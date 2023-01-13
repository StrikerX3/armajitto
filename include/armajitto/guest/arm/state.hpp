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

// Helper class used by the host with offsets for all State fields.
class StateOffsets;

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
    void SetMode(Mode mode);

    // Convenience method that forces the processor to enter the specified exception vector.
    void EnterException(Exception vector);

    // -------------------------------------------------------------------------
    // State accessors

    uint32_t &GPR(enum GPR gpr, Mode mode) {
        const auto index = static_cast<size_t>(gpr) + static_cast<size_t>(mode) * 16;
        assert(static_cast<size_t>(gpr) < 16);
        assert(static_cast<size_t>(mode) < 32);
        return *m_gprPtrs[index];
    }

    uint32_t &GPR(enum GPR gpr) {
        return GPR(gpr, CPSR().mode);
    }

    PSR &CPSR() {
        return m_psrs[0];
    }

    PSR &SPSR(Mode mode) {
        const auto index = static_cast<size_t>(mode);
        assert(index < 32);
        return *m_psrPtrs[index];
    }

    PSR &SPSR() {
        return SPSR(CPSR().mode);
    }

    bool &IRQLine() {
        return m_irqLine;
    }

    ExecState &ExecutionState() {
        return m_execState;
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
    // Const state accessors

    const uint32_t &GPR(enum GPR gpr, Mode mode) const {
        return const_cast<State *>(this)->GPR(gpr, mode);
    }

    const uint32_t &GPR(enum GPR gpr) const {
        return const_cast<State *>(this)->GPR(gpr);
    }

    const PSR &CPSR() const {
        return const_cast<State *>(this)->CPSR();
    }

    const PSR &SPSR(Mode mode) const {
        return const_cast<State *>(this)->SPSR(mode);
    }

    const PSR &SPSR() const {
        return const_cast<State *>(this)->SPSR();
    }

    const bool &IRQLine() const {
        return const_cast<State *>(this)->IRQLine();
    }

    const ExecState &ExecutionState() const {
        return const_cast<State *>(this)->ExecutionState();
    }

    const Coprocessor &GetCoprocessor(uint8_t cpnum) const {
        return const_cast<State *>(this)->GetCoprocessor(cpnum);
    }

    const DummyDebugCoprocessor &GetDummyDebugCoprocessor() const {
        return const_cast<State *>(this)->GetDummyDebugCoprocessor();
    }

    const SystemControlCoprocessor &GetSystemControlCoprocessor() const {
        return const_cast<State *>(this)->GetSystemControlCoprocessor();
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
    // R8..R12 for all modes but FIQ
    // R13 and R14 for all modes but SVC, ABT, IRQ, UND and FIQ
    alignas(16) std::array<uint32_t, 16> m_regsUSR;
    std::array<uint32_t, 14 - 13 + 1> m_regsSVC; // R13 and R14 for SVC
    std::array<uint32_t, 14 - 13 + 1> m_regsABT; // R13 and R14 for ABT
    std::array<uint32_t, 14 - 13 + 1> m_regsIRQ; // R13 and R14 for IRQ
    std::array<uint32_t, 14 - 13 + 1> m_regsUND; // R13 and R14 for UND
    std::array<uint32_t, 14 - 8 + 1> m_regsFIQ;  // R8..R14 for FIQ

    // PSR per mode
    // [0] CPSR
    // [1] SPSR_fiq
    // [2] SPSR_irq
    // [3] SPSR_svc
    // [4] SPSR_abt
    // [5] SPSR_und
    std::array<union PSR, 6> m_psrs;

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

    friend class StateOffsets;
};

} // namespace armajitto::arm
