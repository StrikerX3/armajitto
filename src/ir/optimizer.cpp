#include "armajitto/ir/optimizer.hpp"

#include "optimizer/const_propagation.hpp"
#include "optimizer/dead_store.hpp"

#include <memory>

namespace armajitto::ir {

void Optimize(memory::Allocator &alloc, BasicBlock &block) {
    Emitter emitter{block};

    bool dirty;
    do {
        dirty = false;
        dirty |= alloc.Allocate<ConstPropagationOptimizerPass>(emitter)->Optimize();
        dirty |= alloc.Allocate<DeadStoreEliminationOptimizerPass>(emitter)->Optimize();
    } while (dirty);
}

} // namespace armajitto::ir
