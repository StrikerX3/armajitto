#include "armajitto/ir/optimizer.hpp"

#include "optimizer/arithmetic_ops_coalescence.hpp"
#include "optimizer/bitwise_ops_coalescence.hpp"
#include "optimizer/const_propagation.hpp"
#include "optimizer/dead_flag_value_store_elimination.hpp"
#include "optimizer/dead_gpr_store_elimination.hpp"
#include "optimizer/dead_host_flag_store_elimination.hpp"
#include "optimizer/dead_reg_store_elimination.hpp"
#include "optimizer/dead_var_store_elimination.hpp"
#include "optimizer/host_flags_ops_coalescence.hpp"

#include <memory>

namespace armajitto::ir {

bool Optimize(BasicBlock &block, OptimizerPasses passes, bool repeatWhileDirty) {
    Emitter emitter{block};

    auto bmPasses = BitmaskEnum(passes);
    bool optimized = false;
    bool dirty;
    do {
        dirty = false;
        if (bmPasses.AllOf(OptimizerPasses::ConstantPropagation)) {
            dirty |= std::make_unique<ConstPropagationOptimizerPass>(emitter)->Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::DeadRegisterStoreElimination)) {
            dirty |= std::make_unique<DeadRegisterStoreEliminationOptimizerPass>(emitter)->Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::DeadGPRStoreElimination)) {
            dirty |= std::make_unique<DeadGPRStoreEliminationOptimizerPass>(emitter)->Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::DeadHostFlagStoreElimination)) {
            dirty |= std::make_unique<DeadHostFlagStoreEliminationOptimizerPass>(emitter)->Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::DeadFlagValueStoreElimination)) {
            dirty |= std::make_unique<DeadFlagValueStoreEliminationOptimizerPass>(emitter)->Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::DeadVarStoreElimination)) {
            dirty |= std::make_unique<DeadVarStoreEliminationOptimizerPass>(emitter)->Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::BitwiseOpsCoalescence)) {
            dirty |= std::make_unique<BitwiseOpsCoalescenceOptimizerPass>(emitter)->Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::ArithmeticOpsCoalescence)) {
            dirty |= std::make_unique<ArithmeticOpsCoalescenceOptimizerPass>(emitter)->Optimize();
        }
        if (bmPasses.AllOf(OptimizerPasses::HostFlagsOpsCoalescence)) {
            dirty |= std::make_unique<HostFlagsOpsCoalescenceOptimizerPass>(emitter)->Optimize();
        }
        optimized |= dirty;
    } while (repeatWhileDirty && dirty);
    return optimized;
}

} // namespace armajitto::ir
