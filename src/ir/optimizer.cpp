#include "armajitto/ir/optimizer.hpp"

#include "optimizer/basic_bitwise_peephole_opts.hpp"
#include "optimizer/const_propagation.hpp"
#include "optimizer/dead_store_elimination.hpp"

#include <memory>

namespace armajitto::ir {

void Optimize(memory::Allocator &alloc, BasicBlock &block, OptimizerPasses passes) {
    Emitter emitter{block};

    auto bmPasses = BitmaskEnum(passes);
    bool dirty;
    do {
        dirty = false;
        if (bmPasses.AllOf(OptimizerPasses::ConstantPropagation)) {
            dirty |= alloc.AllocateNonTrivial<ConstPropagationOptimizerPass>(emitter)->Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::DeadStoreElimination)) {
            dirty |= alloc.AllocateNonTrivial<DeadStoreEliminationOptimizerPass>(emitter)->Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::BasicBitwisePeepholeOptimizations)) {
            dirty |= alloc.AllocateNonTrivial<BasicBitwisePeepholeOptimizerPass>(emitter)->Optimize();
        }
    } while (dirty);
}

} // namespace armajitto::ir
