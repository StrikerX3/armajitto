#pragma once

#include "armajitto/core/allocator.hpp"
#include "armajitto/util/bitmask_enum.hpp"
#include "basic_block.hpp"

namespace armajitto::ir {

enum class OptimizerPasses {
    None = 0,

    IdentityOpsElimination = (1 << 0),
    ConstantPropagation = (1 << 1),
    DeadGPRStoreElimination = (1 << 2),
    DeadPSRStoreElimination = (1 << 3),
    DeadHostFlagStoreElimination = (1 << 4),
    DeadFlagValueStoreElimination = (1 << 5),
    DeadVarStoreElimination = (1 << 6),
    BitwiseOpsCoalescence = (1 << 7),
    ArithmeticOpsCoalescence = (1 << 8),
    HostFlagsOpsCoalescence = (1 << 9),

    DeadStoreElimination = DeadGPRStoreElimination | DeadPSRStoreElimination | DeadHostFlagStoreElimination |
                           DeadFlagValueStoreElimination | DeadVarStoreElimination,

    All = IdentityOpsElimination | ConstantPropagation | DeadStoreElimination | BitwiseOpsCoalescence |
          ArithmeticOpsCoalescence | HostFlagsOpsCoalescence,
};

bool Optimize(memory::Allocator &alloc, BasicBlock &block, OptimizerPasses passes = OptimizerPasses::All,
              bool repeatWhileDirty = true);

} // namespace armajitto::ir

ENABLE_BITMASK_OPERATORS(armajitto::ir::OptimizerPasses);
