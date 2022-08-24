#pragma once

#include "armajitto/core/allocator.hpp"
#include "armajitto/util/bitmask_enum.hpp"
#include "basic_block.hpp"

namespace armajitto::ir {

enum class OptimizerPasses {
    None = 0,

    ConstantPropagation = (1 << 0),
    DeadStoreElimination = (1 << 1),
    BitwiseOpsCoalescence = (1 << 2),
    ArithmeticOpsCoalescence = (1 << 3),
    HostFlagsOpsCoalescence = (1 << 4),

    All = ConstantPropagation | DeadStoreElimination | BitwiseOpsCoalescence | ArithmeticOpsCoalescence |
          HostFlagsOpsCoalescence,
};

void Optimize(memory::Allocator &alloc, BasicBlock &block, OptimizerPasses passes = OptimizerPasses::All);

} // namespace armajitto::ir

ENABLE_BITMASK_OPERATORS(armajitto::ir::OptimizerPasses);
