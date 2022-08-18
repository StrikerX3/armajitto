#pragma once

#include <cstdint>
#include <format>
#include <string>

namespace armajitto::arm {

enum class Mode : uint32_t {
    User = 0x10,
    FIQ = 0x11,
    IRQ = 0x12,
    Supervisor = 0x13, // aka SWI
    Abort = 0x17,
    Undefined = 0x1B,
    System = 0x1F
};

inline std::string ToString(Mode mode) {
    switch (mode) {
    case Mode::User: return "usr";
    case Mode::FIQ: return "fiq";
    case Mode::IRQ: return "irq";
    case Mode::Supervisor: return "svc";
    case Mode::Abort: return "abt";
    case Mode::Undefined: return "und";
    case Mode::System: return "sys";
    default: return std::format("0x{:x}", static_cast<uint32_t>(mode));
    }
}

} // namespace armajitto::arm
