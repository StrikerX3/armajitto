#include "armajitto/guest/arm/mode.hpp"

#include <iomanip>
#include <sstream>
#include <string>

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
    default: {
        std::ostringstream oss;
        oss << "unk" << std::setfill('0') << std::setw(2) << std::right << std::hex << std::uppercase
            << static_cast<uint32_t>(mode);
        return oss.str();
    }
    }
}

} // namespace armajitto::arm
