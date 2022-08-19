#include "armajitto/ir/optimizer.hpp"

#include "optimizer/const_propagation.hpp"

namespace armajitto::ir {

void Optimizer::Optimize(BasicBlock &block) {
    Emitter emitter{block};

    ConstPropagationOptimizerPass{emitter}.Optimize();
}

} // namespace armajitto::ir
