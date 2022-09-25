#pragma once

#include "armajitto/guest/arm/mode.hpp"

#include <cstdint>
#include <iomanip>
#include <sstream>

namespace armajitto {

struct LocationRef {
    LocationRef()
        : m_value(0) {}

    LocationRef(uint32_t pc, uint32_t cpsr)
        : m_value(pc, cpsr) {}

    LocationRef(uint32_t pc, arm::Mode mode, bool thumb)
        : m_value(pc, static_cast<uint32_t>(mode) | (static_cast<uint32_t>(thumb) << 5)) {}

    LocationRef(uint64_t key)
        : m_value(key) {}

    uint32_t PC() const {
        return m_value.pc;
    }

    arm::Mode Mode() const {
        return static_cast<arm::Mode>(m_value.cpsr & 0x1F);
    }

    bool IsThumbMode() const {
        return (m_value.cpsr >> 5) & 1;
    }

    uint32_t BaseAddress() const {
        return PC() - (IsThumbMode() ? sizeof(uint16_t) : sizeof(uint32_t)) * 2;
    }

    // Builds a value suitable for use in hash tables.
    //  63          38 37  36      32 31     0
    // |   reserved   | T |   Mode   |   PC   |
    uint64_t ToUint64() const {
        return m_value.u64;
    }

    std::string ToString() const {
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(8) << std::right << std::hex << std::uppercase << PC();
        oss << '_' << arm::ToString(Mode()) << '_' << (IsThumbMode() ? "Thumb" : "ARM");
        return oss.str();
    }

private:
    union Value {
        Value(uint64_t value)
            : u64(value) {
            _zero = 0;
        }

        Value(uint32_t pc, uint32_t cpsr)
            : pc(pc)
            , cpsr(cpsr)
            , _zero(0) {}

        uint64_t u64;
        struct {
            uint64_t pc : 32;
            uint64_t cpsr : 6;
            uint64_t _zero : 26;
        };
    } m_value;
};

} // namespace armajitto
