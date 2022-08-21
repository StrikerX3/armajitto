#include "armajitto/ir/optimizer.hpp"

#include "optimizer/const_propagation.hpp"
#include "optimizer/dead_store_elimination.hpp"

#include <memory>

namespace armajitto::ir {

void Optimize(memory::Allocator &alloc, BasicBlock &block, const OptimizerPasses &passes) {
    Emitter emitter{block};

    bool dirty;
    do {
        dirty = false;
        if (passes.constantPropagation) {
            dirty |= alloc.AllocateNonTrivial<ConstPropagationOptimizerPass>(emitter)->Optimize();
        }
        if (passes.deadStoreElimination) {
            dirty |= alloc.AllocateNonTrivial<DeadStoreEliminationOptimizerPass>(emitter)->Optimize();
        }
    } while (dirty);
}

} // namespace armajitto::ir
