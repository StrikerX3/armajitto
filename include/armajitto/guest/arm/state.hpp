#pragma once

#include "coprocessor.hpp"
#include "gpr.hpp"
#include "mode.hpp"
#include "psr.hpp"

#include "coprocessors/coproc_14_debug_dummy.hpp"
#include "coprocessors/coproc_15_sys_ctrl.hpp"
#include "coprocessors/coproc_null.hpp"

#include <array>
#include <cassert>
#include <cstdint>

namespace armajitto::arm {

class State {
public:
    State();

    void Reset();

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

    void JumpTo(uint32_t address, bool thumb) {
        GPR(GPR::PC) = address + (thumb ? 4 : 8);
        CPSR().t = thumb;
    }

    uintptr_t GPROffset(enum GPR gpr, enum Mode mode) const {
        const auto index = static_cast<size_t>(gpr) + static_cast<size_t>(mode) * 16;
        assert(index < kNumGPREntries);
        return m_gprOffsets[index];
    }

    uintptr_t CPSROffset() const {
        return m_psrOffsets[0];
    }

    uintptr_t SPSROffset(enum Mode mode) const {
        const auto index = static_cast<size_t>(mode);
        assert(index < kNumPSREntries);
        return m_psrOffsets[index];
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

    static constexpr size_t kNumGPREntries = 16 * 32;
    static constexpr size_t kNumPSREntries = 32;

    // Lookup tables of GPRs and PSRs per mode (full range, for fast access)
    std::array<uint32_t *, kNumGPREntries> m_gprPtrs;
    std::array<union PSR *, kNumPSREntries> m_psrPtrs;

    // Lookup tables of GPRs and PSRs offsets
    std::array<uintptr_t, kNumGPREntries> m_gprOffsets;
    std::array<uintptr_t, kNumPSREntries> m_psrOffsets;

    // Coprocessors
    DummyDebugCoprocessor m_cp14;
    SystemControlCoprocessor m_cp15;
};

} // namespace armajitto::arm
