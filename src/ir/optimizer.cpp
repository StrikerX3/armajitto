#include "armajitto/ir/optimizer.hpp"

#include "optimizer/const_propagation.hpp"

namespace armajitto::ir {

void Optimizer::Optimize(BasicBlock &block) {
    Emitter emitter{block};

    bool dirty;
    do {
        dirty = false;
        dirty |= ConstPropagationOptimizerPass{emitter}.Optimize();
    } while (dirty);
}

} // namespace armajitto::ir
