#include "armajitto/ir/optimizer.hpp"

#include "optimizer/const_propagation.hpp"
#include "optimizer/dead_store.hpp"

#include <memory>

namespace armajitto::ir {

void Optimizer::Optimize(BasicBlock &block) {
    Emitter emitter{block};

    bool dirty;
    do {
        dirty = false;
        dirty |= std::make_unique<ConstPropagationOptimizerPass>(emitter)->Optimize();
        dirty |= std::make_unique<DeadStoreEliminationOptimizerPass>(emitter)->Optimize();
    } while (dirty);
}

} // namespace armajitto::ir
