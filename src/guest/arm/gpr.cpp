#include "armajitto/guest/arm/gpr.hpp"

namespace armajitto::arm {

std::string ToString(GPR gpr) {
    static constexpr const char *names[] = {"r0", "r1", "r2",  "r3",  "r4",  "r5", "r6", "r7",
                                            "r8", "r9", "r10", "r11", "r12", "sp", "lr", "pc"};
    return names[static_cast<size_t>(gpr)];
}

} // namespace armajitto::arm
