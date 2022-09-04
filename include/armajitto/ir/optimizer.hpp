#pragma once

#include "basic_block.hpp"

#include <memory_resource>

namespace armajitto::ir {

class Optimizer {
public:
    struct Parameters {
        struct Passes {
            bool constantPropagation = true;

            bool deadRegisterStoreElimination = true;
            bool deadGPRStoreElimination = true;
            bool deadHostFlagStoreElimination = true;
            bool deadFlagValueStoreElimination = true;
            bool deadVariableStoreElimination = true;

            bool bitwiseOpsCoalescence = true;
            bool arithmeticOpsCoalescence = true;
            bool hostFlagsOpsCoalescence = true;
        } passes;

        bool repeatWhileDirty = true;
    };

    Optimizer(std::pmr::memory_resource &pmrBuffer)
        : m_pmrBuffer(pmrBuffer) {}

    Parameters &GetParameters() {
        return m_parameters;
    }

    bool Optimize(BasicBlock &block);

private:
    Parameters m_parameters;

    std::pmr::memory_resource &m_pmrBuffer;
};

} // namespace armajitto::ir
