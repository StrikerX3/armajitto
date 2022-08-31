#pragma once

#include <cstdint>

namespace armajitto::arm {

enum class ExecState : uint8_t {
    Running,
    Halted,
    Stopped,
};

} // namespace armajitto::arm
