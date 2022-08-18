#pragma once

#include "psr.hpp"

#include <array>
#include <cassert>
#include <cstdint>

namespace armajitto::arm {

class State {
public:
    uint32_t &GPR(uint8_t index) {
        assert(index < 16);
        return m_regs[index];
    }

    const uint32_t &GPR(uint8_t index) const {
        return const_cast<State *>(this)->GPR(index);
    }

    PSR &CPSR() {
        return m_cpsr;
    }

    const PSR &CPSR() const {
        return const_cast<State *>(this)->CPSR();
    }

    void JumpTo(uint32_t address, bool thumb) {
        m_regs[15] = address + (thumb ? 4 : 8);
        m_cpsr.t = thumb;
    }

private:
    alignas(16) std::array<uint32_t, 16> m_regs;
    PSR m_cpsr;

    /*std::array<uint32_t, 14 - 13 + 1> m_regsUSR;
    std::array<uint32_t, 14 - 13 + 1> m_regsSVC;
    std::array<uint32_t, 14 - 13 + 1> m_regsABT;
    std::array<uint32_t, 14 - 13 + 1> m_regsIRQ;
    std::array<uint32_t, 14 - 13 + 1> m_regsUND;
    std::array<uint32_t, 14 - 8 + 1> m_regsFIQ;
    std::array<PSR, 6> m_spsr;*/
};

} // namespace armajitto::arm
