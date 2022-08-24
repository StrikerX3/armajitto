#include "armajitto/ir/optimizer.hpp"

#include "optimizer/arithmetic_ops_coalescence.hpp"
#include "optimizer/bitwise_ops_coalescence.hpp"
#include "optimizer/const_propagation.hpp"
#include "optimizer/dead_flag_value_store_elimination.hpp"
#include "optimizer/dead_gpr_store_elimination.hpp"
#include "optimizer/dead_host_flag_store_elimination.hpp"
#include "optimizer/dead_psr_store_elimination.hpp"
#include "optimizer/dead_var_store_elimination.hpp"
#include "optimizer/host_flags_ops_coalescence.hpp"

#include <memory>

namespace armajitto::ir {

bool Optimize(memory::Allocator &alloc, BasicBlock &block, OptimizerPasses passes, bool repeatWhileDirty) {
    Emitter emitter{block};

    auto bmPasses = BitmaskEnum(passes);
    bool optimized = false;
    bool dirty;
    do {
        dirty = false;
        if (bmPasses.AllOf(OptimizerPasses::ConstantPropagation)) {
            dirty |= alloc.AllocateNonTrivial<ConstPropagationOptimizerPass>(emitter)->Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::DeadGPRStoreElimination)) {
            dirty |= alloc.Allocate<DeadGPRStoreEliminationOptimizerPass>(emitter)->Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::DeadPSRStoreElimination)) {
            dirty |= alloc.AllocateNonTrivial<DeadPSRStoreEliminationOptimizerPass>(emitter)->Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::DeadHostFlagStoreElimination)) {
            dirty |= alloc.Allocate<DeadHostFlagStoreEliminationOptimizerPass>(emitter)->Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::DeadFlagValueStoreElimination)) {
            dirty |= alloc.AllocateNonTrivial<DeadFlagValueStoreEliminationOptimizerPass>(emitter)->Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::DeadVarStoreElimination)) {
            dirty |= alloc.AllocateNonTrivial<DeadVarStoreEliminationOptimizerPass>(emitter)->Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::BitwiseOpsCoalescence)) {
            dirty |= alloc.AllocateNonTrivial<BitwiseOpsCoalescenceOptimizerPass>(emitter)->Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::ArithmeticOpsCoalescence)) {
            dirty |= alloc.Allocate<ArithmeticOpsCoalescenceOptimizerPass>(emitter)->Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::HostFlagsOpsCoalescence)) {
            dirty |= alloc.Allocate<HostFlagsOpsCoalescenceOptimizerPass>(emitter)->Optimize();
        }
        optimized |= dirty;
    } while (repeatWhileDirty && dirty);
    return optimized;
}

} // namespace armajitto::ir
