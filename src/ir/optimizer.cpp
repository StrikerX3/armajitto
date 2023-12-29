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
// #include "optimizer/var_lifetime_opt.hpp"

#include "emitter.hpp"
#include "ir/ops/ir_ops_visitor.hpp"

#include <memory_resource>

namespace armajitto::ir {

bool Optimizer::Optimize(BasicBlock &block) {
    bool optimized = DoOptimizations(block);
    DetectIdleLoops(block);
    return optimized;
}

bool Optimizer::DoOptimizations(BasicBlock &block) {
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
    // VarLifetimeOptimizerPass varLifetimeOptimizerPass{emitter, m_pmrBuffer};

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
        /*if (m_options.passes.varLifetimeOptimization) {
            dirty |= varLifetimeOptimizerPass.Optimize();
        }*/
        optimized |= dirty;
        if (!dirty) {
            break;
        }
    };
    return optimized;
}

void Optimizer::DetectIdleLoops(ir::BasicBlock &block) {
    const auto loc = block.Location();
    const uint32_t opcodeSize = loc.IsThumbMode() ? sizeof(uint16_t) : sizeof(uint32_t);
    const uint32_t blockEntryAddress = loc.PC() - opcodeSize * 2;
    enum class Result { Confirmed, Denied, Possible };

    // Check for possible idle loop based on the following rules
    // - Branches back to the beginning of the block
    // - Produces no side-effects, including writes to memory, coprocessor registers or CPSR
    // - Only modifies registers read from within the block
    ir::IROp *op = block.Head();
    while (op != nullptr) {
        uint16_t readRegs = 0;
        uint16_t writtenRegs = 0;
        uint16_t disallowedRegs = 0;
        auto result = VisitIROp(op, [&](auto *op) {
            using TOp = std::decay_t<std::remove_cvref_t<decltype(*op)>>;
            if constexpr (std::is_same_v<TOp, ir::IRBranchOp> || std::is_same_v<TOp, ir::IRBranchExchangeOp>) {
                if (op->address.immediate && op->address.imm.value == blockEntryAddress) {
                    return Result::Confirmed;
                }
            } else if constexpr (std::is_same_v<TOp, ir::IRSetCPSROp>) {
                return Result::Denied;
            } else if constexpr (std::is_same_v<TOp, ir::IRMemWriteOp>) {
                return Result::Denied;
            } else if constexpr (std::is_same_v<TOp, ir::IRStoreCopRegisterOp>) {
                const auto &cop = m_context.GetARMState().GetCoprocessor(op->cpnum);
                if (op->ext && cop.ExtRegStoreHasSideEffects(op->reg)) {
                    return Result::Denied;
                }
                if (!op->ext && cop.RegStoreHasSideEffects(op->reg)) {
                    return Result::Denied;
                }
            } else if constexpr (std::is_same_v<TOp, ir::IRGetRegisterOp>) {
                readRegs |= 1u << static_cast<uint16_t>(op->src.gpr);
            } else if constexpr (std::is_same_v<TOp, ir::IRSetRegisterOp>) {
                const auto reg = 1u << static_cast<uint16_t>(op->dst.gpr);
                if (disallowedRegs & reg) {
                    return Result::Denied;
                }
                writtenRegs |= reg;
                disallowedRegs |= readRegs & ~writtenRegs;
            }
            return Result::Possible;
        });

        auto locStr = loc.ToString();
        if (result == Result::Confirmed) {
            ir::Emitter emitter{block};
            emitter.TerminateIdleLoop();
            break;
        } else if (result == Result::Denied) {
            break;
        }
        op = op->Next();
    }
}

} // namespace armajitto::ir
