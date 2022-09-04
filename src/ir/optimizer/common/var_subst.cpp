#include "var_subst.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

namespace armajitto::ir {

VarSubstitutor::VarSubstitutor(size_t varCount, std::pmr::memory_resource &alloc)
    : m_varSubsts(&alloc) {
    m_varSubsts.resize(varCount);
}

void VarSubstitutor::Assign(VariableArg dst, VariableArg src) {
    if (!dst.var.IsPresent() || !src.var.IsPresent()) {
        return;
    }
    const auto varIndex = dst.var.Index();
    ResizeVarSubsts(varIndex);
    m_varSubsts[varIndex] = src.var;
}

bool VarSubstitutor::Substitute(IROp *op) {
    return VisitIROp(op, [this](auto *op) { return SubstituteImpl(op); });
}

// ---------------------------------------------------------------------------------------------------------------------

void VarSubstitutor::ResizeVarSubsts(size_t index) {
    if (m_varSubsts.size() <= index) {
        m_varSubsts.resize(index + 1);
    }
}

bool VarSubstitutor::Substitute(VariableArg &var) {
    if (!var.var.IsPresent()) {
        return false;
    }

    const auto varIndex = var.var.Index();
    if (varIndex >= m_varSubsts.size()) {
        return false;
    }
    if (!m_varSubsts[varIndex].IsPresent()) {
        return false;
    }
    bool changed = (var != m_varSubsts[varIndex]);
    var = m_varSubsts[varIndex];
    return changed;
}

bool VarSubstitutor::Substitute(VarOrImmArg &var) {
    if (!var.immediate) {
        return Substitute(var.var);
    }
    return false;
}

// ---------------------------------------------------------------------------------------------------------------------

bool VarSubstitutor::SubstituteImpl(IRSetRegisterOp *op) {
    return Substitute(op->src);
}

bool VarSubstitutor::SubstituteImpl(IRSetCPSROp *op) {
    return Substitute(op->src);
}

bool VarSubstitutor::SubstituteImpl(IRSetSPSROp *op) {
    return Substitute(op->src);
}

bool VarSubstitutor::SubstituteImpl(IRMemReadOp *op) {
    return Substitute(op->address);
}

bool VarSubstitutor::SubstituteImpl(IRMemWriteOp *op) {
    bool subst1 = Substitute(op->src);
    bool subst2 = Substitute(op->address);
    return subst1 || subst2;
}

bool VarSubstitutor::SubstituteImpl(IRPreloadOp *op) {
    return Substitute(op->address);
}

bool VarSubstitutor::SubstituteImpl(IRLogicalShiftLeftOp *op) {
    bool subst1 = Substitute(op->value);
    bool subst2 = Substitute(op->amount);
    return subst1 || subst2;
}

bool VarSubstitutor::SubstituteImpl(IRLogicalShiftRightOp *op) {
    bool subst1 = Substitute(op->value);
    bool subst2 = Substitute(op->amount);
    return subst1 || subst2;
}

bool VarSubstitutor::SubstituteImpl(IRArithmeticShiftRightOp *op) {
    bool subst1 = Substitute(op->value);
    bool subst2 = Substitute(op->amount);
    return subst1 || subst2;
}

bool VarSubstitutor::SubstituteImpl(IRRotateRightOp *op) {
    bool subst1 = Substitute(op->value);
    bool subst2 = Substitute(op->amount);
    return subst1 || subst2;
}

bool VarSubstitutor::SubstituteImpl(IRRotateRightExtendedOp *op) {
    return Substitute(op->value);
}

bool VarSubstitutor::SubstituteImpl(IRBitwiseAndOp *op) {
    bool subst1 = Substitute(op->lhs);
    bool subst2 = Substitute(op->rhs);
    return subst1 || subst2;
}

bool VarSubstitutor::SubstituteImpl(IRBitwiseOrOp *op) {
    bool subst1 = Substitute(op->lhs);
    bool subst2 = Substitute(op->rhs);
    return subst1 || subst2;
}

bool VarSubstitutor::SubstituteImpl(IRBitwiseXorOp *op) {
    bool subst1 = Substitute(op->lhs);
    bool subst2 = Substitute(op->rhs);
    return subst1 || subst2;
}

bool VarSubstitutor::SubstituteImpl(IRBitClearOp *op) {
    bool subst1 = Substitute(op->lhs);
    bool subst2 = Substitute(op->rhs);
    return subst1 || subst2;
}

bool VarSubstitutor::SubstituteImpl(IRCountLeadingZerosOp *op) {
    return Substitute(op->value);
}

bool VarSubstitutor::SubstituteImpl(IRAddOp *op) {
    bool subst1 = Substitute(op->lhs);
    bool subst2 = Substitute(op->rhs);
    return subst1 || subst2;
}

bool VarSubstitutor::SubstituteImpl(IRAddCarryOp *op) {
    bool subst1 = Substitute(op->lhs);
    bool subst2 = Substitute(op->rhs);
    return subst1 || subst2;
}

bool VarSubstitutor::SubstituteImpl(IRSubtractOp *op) {
    bool subst1 = Substitute(op->lhs);
    bool subst2 = Substitute(op->rhs);
    return subst1 || subst2;
}

bool VarSubstitutor::SubstituteImpl(IRSubtractCarryOp *op) {
    bool subst1 = Substitute(op->lhs);
    bool subst2 = Substitute(op->rhs);
    return subst1 || subst2;
}

bool VarSubstitutor::SubstituteImpl(IRMoveOp *op) {
    return Substitute(op->value);
}

bool VarSubstitutor::SubstituteImpl(IRMoveNegatedOp *op) {
    return Substitute(op->value);
}

bool VarSubstitutor::SubstituteImpl(IRSaturatingAddOp *op) {
    bool subst1 = Substitute(op->lhs);
    bool subst2 = Substitute(op->rhs);
    return subst1 || subst2;
}

bool VarSubstitutor::SubstituteImpl(IRSaturatingSubtractOp *op) {
    bool subst1 = Substitute(op->lhs);
    bool subst2 = Substitute(op->rhs);
    return subst1 || subst2;
}

bool VarSubstitutor::SubstituteImpl(IRMultiplyOp *op) {
    bool subst1 = Substitute(op->lhs);
    bool subst2 = Substitute(op->rhs);
    return subst1 || subst2;
}

bool VarSubstitutor::SubstituteImpl(IRMultiplyLongOp *op) {
    bool subst1 = Substitute(op->lhs);
    bool subst2 = Substitute(op->rhs);
    return subst1 || subst2;
}

bool VarSubstitutor::SubstituteImpl(IRAddLongOp *op) {
    bool subst1 = Substitute(op->lhsLo);
    bool subst2 = Substitute(op->lhsHi);
    bool subst3 = Substitute(op->rhsLo);
    bool subst4 = Substitute(op->rhsHi);
    return subst1 || subst2 || subst3 || subst4;
}

bool VarSubstitutor::SubstituteImpl(IRStoreFlagsOp *op) {
    return Substitute(op->values);
}

bool VarSubstitutor::SubstituteImpl(IRLoadFlagsOp *op) {
    return Substitute(op->srcCPSR);
}

bool VarSubstitutor::SubstituteImpl(IRLoadStickyOverflowOp *op) {
    return Substitute(op->srcCPSR);
}

bool VarSubstitutor::SubstituteImpl(IRBranchOp *op) {
    return Substitute(op->address);
}

bool VarSubstitutor::SubstituteImpl(IRBranchExchangeOp *op) {
    return Substitute(op->address);
}

bool VarSubstitutor::SubstituteImpl(IRStoreCopRegisterOp *op) {
    return Substitute(op->srcValue);
}

bool VarSubstitutor::SubstituteImpl(IRCopyVarOp *op) {
    return Substitute(op->var);
}

} // namespace armajitto::ir
