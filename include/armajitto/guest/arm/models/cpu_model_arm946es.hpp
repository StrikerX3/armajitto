#pragma once

#include "cpu_model_base.hpp"

namespace armajitto::arm {

class ARM946ESModel : public CPUModelBase<CPUModel::ARM946ES, CPUArch::ARMv5TE> {
public:
    void InitializeState(arm::State &state) final;
};

} // namespace armajitto::arm
