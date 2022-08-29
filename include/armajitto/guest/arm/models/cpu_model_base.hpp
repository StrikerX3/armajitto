#pragma once

#include "armajitto/defs/cpu_arch.hpp"
#include "armajitto/defs/cpu_model.hpp"
#include "armajitto/guest/arm/state.hpp"

namespace armajitto::arm {

template <CPUModel kModel, CPUArch kArch>
class CPUModelBase {
public:
    CPUModel GetCPUModel() const {
        return s_model;
    }

    CPUArch GetCPUArch() const {
        return s_arch;
    }

    virtual void InitializeState(arm::State &state) = 0;

protected:
    static constexpr CPUModel s_model = kModel;
    static constexpr CPUArch s_arch = kArch;
};

} // namespace armajitto::arm
