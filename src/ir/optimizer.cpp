#include "armajitto/ir/optimizer.hpp"

#include "armajitto/core/pmr_allocator.hpp"
#include "optimizer/arithmetic_ops_coalescence.hpp"
#include "optimizer/bitwise_ops_coalescence.hpp"
#include "optimizer/const_propagation.hpp"
#include "optimizer/dead_flag_value_store_elimination.hpp"
#include "optimizer/dead_gpr_store_elimination.hpp"
#include "optimizer/dead_host_flag_store_elimination.hpp"
#include "optimizer/dead_reg_store_elimination.hpp"
#include "optimizer/dead_var_store_elimination.hpp"
#include "optimizer/host_flags_ops_coalescence.hpp"

#include <memory_resource>

namespace armajitto::ir {

bool Optimize(BasicBlock &block, OptimizerPasses passes, bool repeatWhileDirty) {
    memory::PMRAllocator pmrAlloc{};
    std::pmr::monotonic_buffer_resource buffer{&pmrAlloc};

    Emitter emitter{block};

    auto bmPasses = BitmaskEnum(passes);
    bool optimized = false;
    bool dirty;
    do {
        dirty = false;
        if (bmPasses.AllOf(OptimizerPasses::ConstantPropagation)) {
            dirty |= ConstPropagationOptimizerPass{emitter, buffer}.Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::DeadRegisterStoreElimination)) {
            dirty |= DeadRegisterStoreEliminationOptimizerPass{emitter, buffer}.Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::DeadGPRStoreElimination)) {
            dirty |= DeadGPRStoreEliminationOptimizerPass{emitter}.Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::DeadHostFlagStoreElimination)) {
            dirty |= DeadHostFlagStoreEliminationOptimizerPass{emitter}.Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::DeadFlagValueStoreElimination)) {
            dirty |= DeadFlagValueStoreEliminationOptimizerPass{emitter, buffer}.Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::DeadVarStoreElimination)) {
            dirty |= DeadVarStoreEliminationOptimizerPass{emitter, buffer}.Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::BitwiseOpsCoalescence)) {
            dirty |= BitwiseOpsCoalescenceOptimizerPass{emitter, buffer}.Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::ArithmeticOpsCoalescence)) {
            dirty |= ArithmeticOpsCoalescenceOptimizerPass{emitter, buffer}.Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::HostFlagsOpsCoalescence)) {
            dirty |= HostFlagsOpsCoalescenceOptimizerPass{emitter}.Optimize();
        }
        optimized |= dirty;
    } while (repeatWhileDirty && dirty);
    return optimized;
}

} // namespace armajitto::ir
