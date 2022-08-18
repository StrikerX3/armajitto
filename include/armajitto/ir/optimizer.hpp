#pragma once

#include "basic_block.hpp"

namespace armajitto::ir {

class Optimizer {
public:
    void Optimize(BasicBlock &block);

private:
};

} // namespace armajitto::ir
