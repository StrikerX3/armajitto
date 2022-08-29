#include "armajitto/guest/arm/models/cpu_model_arm946es.hpp"

namespace armajitto::arm {

void ARM946ESModel::InitializeState(arm::State &state) {
    SystemControlCoprocessor::Parameters cp15Params{
        .itcmSize = 32 * 1024,
        .dtcmSize = 16 * 1024,
    };
    state.GetSystemControlCoprocessor().Install(cp15Params);
}

} // namespace armajitto::arm
