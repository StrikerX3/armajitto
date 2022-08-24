#include "identity_ops_elimination.hpp"

namespace armajitto::ir {

IdentityOpsEliminationOptimizerPass::IdentityOpsEliminationOptimizerPass(Emitter &emitter)
    : OptimizerPassBase(emitter)
    , m_varSubst(emitter.VariableCount()) {}

void IdentityOpsEliminationOptimizerPass::PreProcess(IROp *op) {
    MarkDirty(m_varSubst.Substitute(op));
}

void IdentityOpsEliminationOptimizerPass::Process(IRLogicalShiftLeftOp *op) {
    ProcessShift(op->dst, op->value, op->amount, op->setCarry, op);
}

void IdentityOpsEliminationOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    ProcessShift(op->dst, op->value, op->amount, op->setCarry, op);
}

void IdentityOpsEliminationOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    ProcessShift(op->dst, op->value, op->amount, op->setCarry, op);
}

void IdentityOpsEliminationOptimizerPass::Process(IRRotateRightOp *op) {
    ProcessShift(op->dst, op->value, op->amount, op->setCarry, op);
}

void IdentityOpsEliminationOptimizerPass::Process(IRBitwiseAndOp *op) {
    ProcessImmVarPair(op->dst, op->lhs, op->rhs, op->flags, ~0, op);
}

void IdentityOpsEliminationOptimizerPass::Process(IRBitwiseOrOp *op) {
    ProcessImmVarPair(op->dst, op->lhs, op->rhs, op->flags, 0, op);
}

void IdentityOpsEliminationOptimizerPass::Process(IRBitwiseXorOp *op) {
    ProcessImmVarPair(op->dst, op->lhs, op->rhs, op->flags, 0, op);
}

void IdentityOpsEliminationOptimizerPass::Process(IRBitClearOp *op) {
    ProcessImmVarPair(op->dst, op->lhs, op->rhs, op->flags, 0, op);
}

void IdentityOpsEliminationOptimizerPass::Process(IRAddOp *op) {
    ProcessImmVarPair(op->dst, op->lhs, op->rhs, op->flags, 0, op);
}

void IdentityOpsEliminationOptimizerPass::Process(IRAddCarryOp *op) {
    // TODO: track host carry flag
    // TODO: extract this functionality from the other optimizer that has it
}

void IdentityOpsEliminationOptimizerPass::Process(IRSubtractOp *op) {
    ProcessImmVarPair(op->dst, op->lhs, op->rhs, op->flags, 0, op);
}

void IdentityOpsEliminationOptimizerPass::Process(IRSubtractCarryOp *op) {
    // TODO: track host carry flag
    // TODO: extract this functionality from the other optimizer that has it
}

void IdentityOpsEliminationOptimizerPass::Process(IRSaturatingAddOp *op) {
    ProcessImmVarPair(op->dst, op->lhs, op->rhs, op->flags, 0, op);
}

void IdentityOpsEliminationOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    ProcessImmVarPair(op->dst, op->lhs, op->rhs, op->flags, 0, op);
}

void IdentityOpsEliminationOptimizerPass::Process(IRMultiplyOp *op) {
    ProcessImmVarPair(op->dst, op->lhs, op->rhs, op->flags, 1, op);
}

void IdentityOpsEliminationOptimizerPass::Process(IRMultiplyLongOp *op) {
    if (op->flags != arm::Flags::None) {
        return;
    }
    if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
        auto [imm, var] = *split;
        if (imm == 1) {
            m_varSubst.Assign(op->dstLo, var);
            m_emitter.Overwrite().Constant(op->dstHi, 0);
        }
    }
}

void IdentityOpsEliminationOptimizerPass::Process(IRAddLongOp *op) {
    if (op->flags != arm::Flags::None) {
        return;
    }
    auto splitLo = SplitImmVarPair(op->lhsLo, op->rhsLo);
    auto splitHi = SplitImmVarPair(op->lhsHi, op->rhsHi);
    if (splitLo && splitHi) {
        auto [immLo, varLo] = *splitLo;
        auto [immHi, varHi] = *splitHi;
        if (immLo == 0 && immHi == 0) {
            m_varSubst.Assign(op->dstLo, varLo);
            m_varSubst.Assign(op->dstHi, varHi);
            m_emitter.Erase(op);
        }
    }
}

void IdentityOpsEliminationOptimizerPass::ProcessShift(const VariableArg &dst, const VarOrImmArg &value,
                                                       const VarOrImmArg &amount, bool setCarry, IROp *op) {
    if (setCarry) {
        return;
    }
    if (!value.immediate && amount.immediate) {
        if (amount.imm.value == 0) {
            m_varSubst.Assign(dst, value.var);
            m_emitter.Erase(op);
        }
    }
}

void IdentityOpsEliminationOptimizerPass::ProcessImmVarPair(const VariableArg &dst, const VarOrImmArg &lhs,
                                                            const VarOrImmArg &rhs, arm::Flags flags,
                                                            uint32_t identityValue, IROp *op) {
    if (flags != arm::Flags::None) {
        return;
    }
    if (auto split = SplitImmVarPair(lhs, rhs)) {
        auto [imm, var] = *split;
        if (imm == identityValue) {
            m_varSubst.Assign(dst, var);
            m_emitter.Erase(op);
        }
    }
}

} // namespace armajitto::ir
