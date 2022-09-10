#pragma once

#include "armajitto/guest/arm/mode.hpp"

#include <cstdint>

namespace armajitto {

struct LocationRef {
    LocationRef()
        : m_pc(0)
        , m_cpsr(0) {}

    LocationRef(uint32_t pc, uint32_t cpsr)
        : m_pc(pc)
        , m_cpsr(cpsr & kCPSRMask) {}

    LocationRef(uint32_t pc, arm::Mode mode, bool thumb)
        : m_pc(pc)
        , m_cpsr(static_cast<uint32_t>(mode) | (static_cast<uint32_t>(thumb) << 5)) {}

    uint32_t PC() const {
        return m_pc;
    }

    arm::Mode Mode() const {
        return static_cast<arm::Mode>(m_cpsr & 0x1F);
    }

    bool IsThumbMode() const {
        return (m_cpsr >> 5) & 1;
    }

    // Builds a value suitable for use in hash tables.
    //  63          38 37  36      32 31     0
    // |   reserved   | T |   Mode   |   PC   |
    uint64_t ToUint64() const {
        return static_cast<uint64_t>(m_pc) | (static_cast<uint64_t>(m_cpsr) << 32ull);
    }

    std::string ToString() const {
        return std::format("{:08X}_{}_{}", m_pc, arm::ToString(Mode()), (IsThumbMode() ? "Thumb" : "ARM"));
    }

private:
    static constexpr uint32_t kCPSRMask = 0x0000003F; // T bit and mode

    uint32_t m_pc;
    uint32_t m_cpsr;
};

} // namespace armajitto
