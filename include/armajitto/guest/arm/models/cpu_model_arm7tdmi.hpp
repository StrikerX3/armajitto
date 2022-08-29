#pragma once

#include "cpu_model_base.hpp"

namespace armajitto::arm {

class ARM7TDMIModel : public CPUModelBase<CPUModel::ARM7TDMI, CPUArch::ARMv5TE> {
public:
    void InitializeState(arm::State &state) final;
};

} // namespace armajitto::arm
