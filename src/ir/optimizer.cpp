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

bool Optimize(BasicBlock &block, const OptimizationParams &params) {
    memory::Allocator alloc{};
    memory::PMRRefAllocator pmrAlloc{alloc};
    return Optimize(pmrAlloc, block, params);
}

bool Optimize(memory::PMRRefAllocator &alloc, BasicBlock &block, const OptimizationParams &params) {
    std::pmr::monotonic_buffer_resource buffer{&alloc};

    Emitter emitter{block};

    bool optimized = false;
    bool dirty;
    do {
        dirty = false;
        if (params.passes.constantPropagation) {
            dirty |= ConstPropagationOptimizerPass{emitter, buffer}.Optimize();
        }
        if (params.passes.deadRegisterStoreElimination) {
            dirty |= DeadRegisterStoreEliminationOptimizerPass{emitter, buffer}.Optimize();
        }
        if (params.passes.deadGPRStoreElimination) {
            dirty |= DeadGPRStoreEliminationOptimizerPass{emitter}.Optimize();
        }
        if (params.passes.deadHostFlagStoreElimination) {
            dirty |= DeadHostFlagStoreEliminationOptimizerPass{emitter}.Optimize();
        }
        if (params.passes.deadFlagValueStoreElimination) {
            dirty |= DeadFlagValueStoreEliminationOptimizerPass{emitter, buffer}.Optimize();
        }
        if (params.passes.deadVariableStoreElimination) {
            dirty |= DeadVarStoreEliminationOptimizerPass{emitter, buffer}.Optimize();
        }
        if (params.passes.bitwiseOpsCoalescence) {
            dirty |= BitwiseOpsCoalescenceOptimizerPass{emitter, buffer}.Optimize();
        }
        if (params.passes.arithmeticOpsCoalescence) {
            dirty |= ArithmeticOpsCoalescenceOptimizerPass{emitter, buffer}.Optimize();
        }
        if (params.passes.hostFlagsOpsCoalescence) {
            dirty |= HostFlagsOpsCoalescenceOptimizerPass{emitter}.Optimize();
        }
        optimized |= dirty;
    } while (params.repeatWhileDirty && dirty);
    return optimized;
}

} // namespace armajitto::ir
