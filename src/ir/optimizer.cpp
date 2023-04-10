#include "optimizer.hpp"

#include "optimizer/arithmetic_ops_coalescence.hpp"
#include "optimizer/bitwise_ops_coalescence.hpp"
#include "optimizer/const_propagation.hpp"
#include "optimizer/dead_flag_value_store_elimination.hpp"
#include "optimizer/dead_gpr_store_elimination.hpp"
#include "optimizer/dead_host_flag_store_elimination.hpp"
#include "optimizer/dead_reg_store_elimination.hpp"
#include "optimizer/dead_var_store_elimination.hpp"
#include "optimizer/host_flags_ops_coalescence.hpp"
#include "optimizer/var_lifetime_opt.hpp"

#include "emitter.hpp"

#include <memory_resource>

namespace armajitto::ir {

bool Optimizer::Optimize(BasicBlock &block) {
    Emitter emitter{block};
    ConstPropagationOptimizerPass constPropPass{emitter, m_pmrBuffer};
    DeadRegisterStoreEliminationOptimizerPass deadRegStoreElimPass{emitter, m_pmrBuffer};
    DeadGPRStoreEliminationOptimizerPass deadGPRStoreElimPass{emitter};
    DeadHostFlagStoreEliminationOptimizerPass deadHostFlagStoreElimPass{emitter};
    DeadFlagValueStoreEliminationOptimizerPass deadFlagValueStoreElimPass{emitter, m_pmrBuffer};
    DeadVarStoreEliminationOptimizerPass deadVarStoreElimPass{emitter, m_pmrBuffer};
    BitwiseOpsCoalescenceOptimizerPass bitwiseCoalescencePass{emitter, m_pmrBuffer};
    ArithmeticOpsCoalescenceOptimizerPass arithmeticCoalescencePass{emitter, m_pmrBuffer};
    HostFlagsOpsCoalescenceOptimizerPass hostFlagsCoalescencePass{emitter};
    VarLifetimeOptimizerPass varLifetimeOptimizerPass{emitter, m_pmrBuffer};

    bool optimized = false;
    int maxIters = m_options.maxIterations;
    for (int i = 0; i < maxIters; i++) {
        bool dirty = false;
        if (m_options.passes.constantPropagation) {
            dirty |= constPropPass.Optimize();
        }
        if (m_options.passes.deadRegisterStoreElimination) {
            dirty |= deadRegStoreElimPass.Optimize();
        }
        if (m_options.passes.deadGPRStoreElimination) {
            dirty |= deadGPRStoreElimPass.Optimize();
        }
        if (m_options.passes.deadHostFlagStoreElimination) {
            dirty |= deadHostFlagStoreElimPass.Optimize();
        }
        if (m_options.passes.deadFlagValueStoreElimination) {
            dirty |= deadFlagValueStoreElimPass.Optimize();
        }
        if (m_options.passes.deadVariableStoreElimination) {
            dirty |= deadVarStoreElimPass.Optimize();
        }
        if (m_options.passes.bitwiseOpsCoalescence) {
            dirty |= bitwiseCoalescencePass.Optimize();
        }
        if (m_options.passes.arithmeticOpsCoalescence) {
            dirty |= arithmeticCoalescencePass.Optimize();
        }
        if (m_options.passes.hostFlagsOpsCoalescence) {
            dirty |= hostFlagsCoalescencePass.Optimize();
        }
        if (m_options.passes.varLifetimeOptimization) {
            dirty |= varLifetimeOptimizerPass.Optimize();
        }
        optimized |= dirty;
        if (!dirty) {
            break;
        }
    };
    return optimized;
}

} // namespace armajitto::ir
