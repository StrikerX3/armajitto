#include "armajitto/guest/arm/coprocessors/cp15/cp15_control.hpp"

namespace armajitto::arm::cp15 {

void ControlRegister::Reset() {
    value.u32 = 0x2078;
    baseVectorAddress = 0xFFFF0000;
}

void ControlRegister::Write(uint32_t value) {
    this->value.u32 = (this->value.u32 & ~0x000FF085) | (value & 0x000FF085);
    // TODO: check for big-endian mode, support it if needed
    baseVectorAddress = (this->value.v) ? 0xFFFF0000 : 0x00000000;
}

} // namespace armajitto::arm::cp15
