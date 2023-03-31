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

#include "emitter.hpp"

#include <memory_resource>

namespace armajitto::ir {

#ifdef _DEBUG
constexpr bool printBlocks = true;
#else
constexpr bool printBlocks = false;
#endif

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

    bool optimized = false;
    int maxIters = m_options.maxIterations;
    for (int i = 0; i < maxIters; i++) {
        bool dirty = false;
        if (m_options.passes.constantPropagation) {
            bool changed = constPropPass.Optimize();
            dirty |= changed;
            if constexpr (printBlocks) {
                if (changed) {
                    printf("\nafter constant propagation:\n");
                    for (auto *op = block.Head(); op != nullptr; op = op->Next()) {
                        auto str = op->ToString();
                        printf("  %s\n", str.c_str());
                    }
                }
            }
        }
        if (m_options.passes.deadRegisterStoreElimination) {
            bool changed = deadRegStoreElimPass.Optimize();
            dirty |= changed;
            if constexpr (printBlocks) {
                if (changed) {
                    printf("\nafter dead register store elimination:\n");
                    for (auto *op = block.Head(); op != nullptr; op = op->Next()) {
                        auto str = op->ToString();
                        printf("  %s\n", str.c_str());
                    }
                }
            }
        }
        if (m_options.passes.deadGPRStoreElimination) {
            bool changed = deadGPRStoreElimPass.Optimize();
            dirty |= changed;
            if constexpr (printBlocks) {
                if (changed) {
                    printf("\nafter dead GPR store elimination:\n");
                    for (auto *op = block.Head(); op != nullptr; op = op->Next()) {
                        auto str = op->ToString();
                        printf("  %s\n", str.c_str());
                    }
                }
            }
        }
        if (m_options.passes.deadHostFlagStoreElimination) {
            bool changed = deadHostFlagStoreElimPass.Optimize();
            dirty |= changed;
            if constexpr (printBlocks) {
                if (changed) {
                    printf("\nafter dead host flags store elimination:\n");
                    for (auto *op = block.Head(); op != nullptr; op = op->Next()) {
                        auto str = op->ToString();
                        printf("  %s\n", str.c_str());
                    }
                }
            }
        }
        if (m_options.passes.deadFlagValueStoreElimination) {
            bool changed = deadFlagValueStoreElimPass.Optimize();
            dirty |= changed;
            if constexpr (printBlocks) {
                if (changed) {
                    printf("\nafter dead flag value store elimination:\n");
                    for (auto *op = block.Head(); op != nullptr; op = op->Next()) {
                        auto str = op->ToString();
                        printf("  %s\n", str.c_str());
                    }
                }
            }
        }
        if (m_options.passes.deadVariableStoreElimination) {
            bool changed = deadVarStoreElimPass.Optimize();
            dirty |= changed;
            if constexpr (printBlocks) {
                if (changed) {
                    printf("\nafter dead var store elimination:\n");
                    for (auto *op = block.Head(); op != nullptr; op = op->Next()) {
                        auto str = op->ToString();
                        printf("  %s\n", str.c_str());
                    }
                }
            }
        }
        if (m_options.passes.bitwiseOpsCoalescence) {
            bool changed = bitwiseCoalescencePass.Optimize();
            dirty |= changed;
            if constexpr (printBlocks) {
                if (changed) {
                    printf("\nafter bitwise ops coalescence:\n");
                    for (auto *op = block.Head(); op != nullptr; op = op->Next()) {
                        auto str = op->ToString();
                        printf("  %s\n", str.c_str());
                    }
                }
            }
        }
        if (m_options.passes.arithmeticOpsCoalescence) {
            bool changed = arithmeticCoalescencePass.Optimize();
            dirty |= changed;
            if constexpr (printBlocks) {
                if (changed) {
                    printf("\nafter arithmetic ops coalescence:\n");
                    for (auto *op = block.Head(); op != nullptr; op = op->Next()) {
                        auto str = op->ToString();
                        printf("  %s\n", str.c_str());
                    }
                }
            }
        }
        if (m_options.passes.hostFlagsOpsCoalescence) {
            bool changed = hostFlagsCoalescencePass.Optimize();
            dirty |= changed;
            if constexpr (printBlocks) {
                if (changed) {
                    printf("\nafter host flags ops coalescence:\n");
                    for (auto *op = block.Head(); op != nullptr; op = op->Next()) {
                        auto str = op->ToString();
                        printf("  %s\n", str.c_str());
                    }
                }
            }
        }
        optimized |= dirty;
        if (!dirty) {
            break;
        }
    };
    return optimized;
}

} // namespace armajitto::ir
