#pragma once

#include "armajitto/core/allocator.hpp"
#include "armajitto/core/pmr_allocator.hpp"
#include "armajitto/util/bitmask_enum.hpp"
#include "basic_block.hpp"

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

bool Optimize(BasicBlock &block, const OptimizationParams &params = {});
bool Optimize(memory::PMRRefAllocator &alloc, BasicBlock &block, const OptimizationParams &params = {});

} // namespace armajitto::ir
