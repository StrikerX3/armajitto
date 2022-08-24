#pragma once

#include "armajitto/core/allocator.hpp"
#include "armajitto/util/bitmask_enum.hpp"
#include "basic_block.hpp"

namespace armajitto::ir {

enum class OptimizerPasses {
    None = 0,

    ConstantPropagation = (1 << 0),
    DeadGPRStoreElimination = (1 << 2),
    DeadPSRStoreElimination = (1 << 3),
    DeadHostFlagStoreElimination = (1 << 4),
    DeadFlagValueStoreElimination = (1 << 5),
    DeadVarStoreElimination = (1 << 1),
    BitwiseOpsCoalescence = (1 << 6),
    ArithmeticOpsCoalescence = (1 << 7),
    HostFlagsOpsCoalescence = (1 << 8),

    DeadStoreElimination = DeadGPRStoreElimination | DeadPSRStoreElimination | DeadHostFlagStoreElimination |
                           DeadFlagValueStoreElimination | DeadVarStoreElimination,

    All = ConstantPropagation | DeadStoreElimination | BitwiseOpsCoalescence | ArithmeticOpsCoalescence |
          HostFlagsOpsCoalescence,
};

void Optimize(memory::Allocator &alloc, BasicBlock &block, OptimizerPasses passes = OptimizerPasses::All);

} // namespace armajitto::ir

ENABLE_BITMASK_OPERATORS(armajitto::ir::OptimizerPasses);
