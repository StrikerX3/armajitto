#pragma once

#include "armajitto/util/bitmask_enum.hpp"
#include "basic_block.hpp"

namespace armajitto::ir {

enum class OptimizerPasses {
    None = 0,

    ConstantPropagation = (1 << 0),
    DeadRegisterStoreElimination = (1 << 1),
    DeadGPRStoreElimination = (1 << 2),
    DeadHostFlagStoreElimination = (1 << 3),
    DeadFlagValueStoreElimination = (1 << 4),
    DeadVarStoreElimination = (1 << 5),
    BitwiseOpsCoalescence = (1 << 6),
    ArithmeticOpsCoalescence = (1 << 7),
    HostFlagsOpsCoalescence = (1 << 8),

    DeadStoreElimination = DeadRegisterStoreElimination | DeadGPRStoreElimination | DeadHostFlagStoreElimination |
                           DeadFlagValueStoreElimination | DeadVarStoreElimination,

    All = ConstantPropagation | DeadStoreElimination | BitwiseOpsCoalescence | ArithmeticOpsCoalescence |
          HostFlagsOpsCoalescence,
};

bool Optimize(BasicBlock &block, OptimizerPasses passes = OptimizerPasses::All, bool repeatWhileDirty = true);

} // namespace armajitto::ir

ENABLE_BITMASK_OPERATORS(armajitto::ir::OptimizerPasses);
