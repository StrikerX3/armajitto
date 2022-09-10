#include "armajitto/guest/arm/mode.hpp"

#include <format>

namespace armajitto::arm {

std::string ToString(Mode mode) {
    switch (mode) {
    case Mode::User: return "usr";
    case Mode::FIQ: return "fiq";
    case Mode::IRQ: return "irq";
    case Mode::Supervisor: return "svc";
    case Mode::Abort: return "abt";
    case Mode::Undefined: return "und";
    case Mode::System: return "sys";
    default: return std::format("unk{:x}", static_cast<uint32_t>(mode));
    }
}

} // namespace armajitto::arm
