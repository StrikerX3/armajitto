#include "armajitto/ir/optimizer.hpp"

#include "optimizer/arithmetic_ops_coalescence.hpp"
#include "optimizer/bitwise_ops_coalescence.hpp"
#include "optimizer/const_propagation.hpp"
#include "optimizer/dead_store_elimination.hpp"
#include "optimizer/host_flags_ops_coalescence.hpp"

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
        if (bmPasses.AllOf(OptimizerPasses::BitwiseOpsCoalescence)) {
            dirty |= alloc.AllocateNonTrivial<BitwiseOpsCoalescenceOptimizerPass>(emitter)->Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::ArithmeticOpsCoalescence)) {
            dirty |= alloc.AllocateNonTrivial<ArithmeticOpsCoalescenceOptimizerPass>(emitter)->Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::HostFlagsOpsCoalescence)) {
            dirty |= alloc.Allocate<HostFlagsOpsCoalescenceOptimizerPass>(emitter)->Optimize();
        }
    } while (dirty);
}

} // namespace armajitto::ir
