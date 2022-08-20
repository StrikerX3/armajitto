#pragma once

#include "armajitto/core/allocator.hpp"
#include "basic_block.hpp"

namespace armajitto::ir {

void Optimize(memory::Allocator &alloc, BasicBlock &block);

} // namespace armajitto::ir
