#include "armajitto/guest/arm/models/cpu_model_arm7tdmi.hpp"

namespace armajitto::arm {

void ARM7TDMIModel::InitializeState(arm::State &state) {
    state.GetDummyDebugCoprocessor().Install();
}

} // namespace armajitto::arm
