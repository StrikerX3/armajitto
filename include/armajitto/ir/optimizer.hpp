#pragma once

#include "basic_block.hpp"

#include <memory_resource>

namespace armajitto::ir {

struct OptimizationParams {
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

bool Optimize(std::pmr::memory_resource &alloc, BasicBlock &block, const OptimizationParams &params = {});

} // namespace armajitto::ir
