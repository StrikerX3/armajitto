#include "armajitto/guest/arm/coprocessors/cp15/cp15_pu.hpp"

namespace armajitto::arm::cp15 {

void ProtectionUnit::Reset() {
    dataCachabilityBits = 0;
    codeCachabilityBits = 0;
    bufferabilityBits = 0;
    dataAccessPermissions = 0;
    codeAccessPermissions = 0;
    for (size_t i = 0; i < 8; i++) {
        regions[i].u32 = 0;
    }
}

} // namespace armajitto::arm::cp15
