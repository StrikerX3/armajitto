#pragma once

#include "armajitto/defs/arm/mode.hpp"
#include "armajitto/util/bit_ops.hpp"

#include <cstdint>

namespace armajitto::ir {

struct LocationRef {
    LocationRef(uint32_t baseAddress, uint32_t cpsr)
        : m_baseAddress(baseAddress)
        , m_cpsr(cpsr & kCPSRMask) {}

    LocationRef(uint32_t baseAddress, arm::Mode mode, bool thumb)
        : m_baseAddress(baseAddress)
        , m_cpsr(static_cast<uint32_t>(mode) | (static_cast<uint32_t>(thumb) << 5)) {}

    uint32_t BaseAddress() const {
        return m_baseAddress;
    }

    arm::Mode Mode() const {
        return static_cast<arm::Mode>(m_cpsr & 0x1F);
    }

    bool IsThumbMode() const {
        return bit::test<5>(m_cpsr);
    }

private:
    static constexpr uint32_t kCPSRMask = 0x0000003F; // T bit and mode

    uint32_t m_baseAddress;
    uint32_t m_cpsr;
};

} // namespace armajitto::ir
