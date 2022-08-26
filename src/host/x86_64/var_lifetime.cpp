#include "var_lifetime.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

namespace armajitto::x86_64 {

void VarLifetimeTracker::Analyze(const ir::BasicBlock &block) {
    m_lastVarReadOps.clear();
    m_lastVarReadOps.resize(block.VariableCount());

    auto *op = block.Head();
    while (op != nullptr) {
        ir::VisitIROp(op, [this](const auto *op) -> void { ComputeVarLifetimes(op); });
        op = op->Next();
    }
}

bool VarLifetimeTracker::IsEndOfLife(ir::Variable var, const ir::IROp *op) const {
    if (!var.IsPresent()) {
        return false;
    }
    return m_lastVarReadOps[var.Index()] == op;
}

// ---------------------------------------------------------------------------------------------------------------------

void VarLifetimeTracker::SetLastVarReadOp(const ir::VariableArg &arg, const ir::IROp *op) {
    if (arg.var.IsPresent()) {
        const auto index = arg.var.Index();
        m_lastVarReadOps[index] = op;
    }
}

void VarLifetimeTracker::SetLastVarReadOp(const ir::VarOrImmArg &arg, const ir::IROp *op) {
    if (!arg.immediate) {
        SetLastVarReadOp(arg.var, op);
    }
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRSetRegisterOp *op) {
    SetLastVarReadOp(op->src, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRSetCPSROp *op) {
    SetLastVarReadOp(op->src, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRSetSPSROp *op) {
    SetLastVarReadOp(op->src, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRMemReadOp *op) {
    SetLastVarReadOp(op->address, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRMemWriteOp *op) {
    SetLastVarReadOp(op->src, op);
    SetLastVarReadOp(op->address, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRPreloadOp *op) {
    SetLastVarReadOp(op->address, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRLogicalShiftLeftOp *op) {
    SetLastVarReadOp(op->value, op);
    SetLastVarReadOp(op->amount, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRLogicalShiftRightOp *op) {
    SetLastVarReadOp(op->value, op);
    SetLastVarReadOp(op->amount, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRArithmeticShiftRightOp *op) {
    SetLastVarReadOp(op->value, op);
    SetLastVarReadOp(op->amount, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRRotateRightOp *op) {
    SetLastVarReadOp(op->value, op);
    SetLastVarReadOp(op->amount, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRRotateRightExtendedOp *op) {
    SetLastVarReadOp(op->value, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRBitwiseAndOp *op) {
    SetLastVarReadOp(op->lhs, op);
    SetLastVarReadOp(op->rhs, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRBitwiseOrOp *op) {
    SetLastVarReadOp(op->lhs, op);
    SetLastVarReadOp(op->rhs, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRBitwiseXorOp *op) {
    SetLastVarReadOp(op->lhs, op);
    SetLastVarReadOp(op->rhs, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRBitClearOp *op) {
    SetLastVarReadOp(op->lhs, op);
    SetLastVarReadOp(op->rhs, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRCountLeadingZerosOp *op) {
    SetLastVarReadOp(op->value, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRAddOp *op) {
    SetLastVarReadOp(op->lhs, op);
    SetLastVarReadOp(op->rhs, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRAddCarryOp *op) {
    SetLastVarReadOp(op->lhs, op);
    SetLastVarReadOp(op->rhs, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRSubtractOp *op) {
    SetLastVarReadOp(op->lhs, op);
    SetLastVarReadOp(op->rhs, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRSubtractCarryOp *op) {
    SetLastVarReadOp(op->lhs, op);
    SetLastVarReadOp(op->rhs, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRMoveOp *op) {
    SetLastVarReadOp(op->value, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRMoveNegatedOp *op) {
    SetLastVarReadOp(op->value, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRSaturatingAddOp *op) {
    SetLastVarReadOp(op->lhs, op);
    SetLastVarReadOp(op->rhs, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRSaturatingSubtractOp *op) {
    SetLastVarReadOp(op->lhs, op);
    SetLastVarReadOp(op->rhs, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRMultiplyOp *op) {
    SetLastVarReadOp(op->lhs, op);
    SetLastVarReadOp(op->rhs, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRMultiplyLongOp *op) {
    SetLastVarReadOp(op->lhs, op);
    SetLastVarReadOp(op->rhs, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRAddLongOp *op) {
    SetLastVarReadOp(op->lhsLo, op);
    SetLastVarReadOp(op->lhsHi, op);
    SetLastVarReadOp(op->rhsLo, op);
    SetLastVarReadOp(op->rhsHi, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRStoreFlagsOp *op) {
    SetLastVarReadOp(op->values, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRLoadFlagsOp *op) {
    SetLastVarReadOp(op->srcCPSR, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRLoadStickyOverflowOp *op) {
    SetLastVarReadOp(op->srcCPSR, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRBranchOp *op) {
    SetLastVarReadOp(op->address, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRBranchExchangeOp *op) {
    SetLastVarReadOp(op->address, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRStoreCopRegisterOp *op) {
    SetLastVarReadOp(op->srcValue, op);
}

void VarLifetimeTracker::ComputeVarLifetimes(const ir::IRCopyVarOp *op) {
    SetLastVarReadOp(op->var, op);
}

} // namespace armajitto::x86_64
