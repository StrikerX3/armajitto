#pragma once

#include "armajitto/core/allocator.hpp"
#include "basic_block.hpp"

namespace armajitto::ir {

// TODO: redesign this
struct OptimizerPasses {
    bool constantPropagation = true;
    bool deadStoreElimination = true;
};

void Optimize(memory::Allocator &alloc, BasicBlock &block, const OptimizerPasses &passes = {});

} // namespace armajitto::ir
