#include "dead_store_elimination_base.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

#include <cassert>

namespace armajitto::ir {

DeadStoreEliminationOptimizerPassBase::DeadStoreEliminationOptimizerPassBase(Emitter &emitter)
    : OptimizerPassBase(emitter) {}

void DeadStoreEliminationOptimizerPassBase::PostProcess() {
    PostProcessImpl();

    // Erase all dead instructions
    m_emitter.GoToHead();
    while (IROp *op = m_emitter.GetCurrentOp()) {
        VisitIROp(op, [this](auto op) -> void {
            if (IsDeadInstruction(op)) {
                m_emitter.Erase(op);
            }
        });
        m_emitter.NextOp();
    }
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRGetRegisterOp *op) {
    return !op->dst.var.IsPresent();
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRGetCPSROp *op) {
    return !op->dst.var.IsPresent();
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRGetSPSROp *op) {
    return !op->dst.var.IsPresent();
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRMemReadOp *op) {
    return !op->dst.var.IsPresent() && op->address.immediate && false /* TODO: no side effects on address */;
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRLogicalShiftLeftOp *op) {
    return !op->dst.var.IsPresent() && !op->setCarry;
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRLogicalShiftRightOp *op) {
    return !op->dst.var.IsPresent() && !op->setCarry;
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRArithmeticShiftRightOp *op) {
    return !op->dst.var.IsPresent() && !op->setCarry;
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRRotateRightOp *op) {
    return !op->dst.var.IsPresent() && !op->setCarry;
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRRotateRightExtendedOp *op) {
    return !op->dst.var.IsPresent() && !op->setCarry;
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRBitwiseAndOp *op) {
    return !op->dst.var.IsPresent() && op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRBitwiseOrOp *op) {
    return !op->dst.var.IsPresent() && op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRBitwiseXorOp *op) {
    return !op->dst.var.IsPresent() && op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRBitClearOp *op) {
    return !op->dst.var.IsPresent() && op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRCountLeadingZerosOp *op) {
    return !op->dst.var.IsPresent();
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRAddOp *op) {
    return !op->dst.var.IsPresent() && op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRAddCarryOp *op) {
    return !op->dst.var.IsPresent() && op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRSubtractOp *op) {
    return !op->dst.var.IsPresent() && op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRSubtractCarryOp *op) {
    return !op->dst.var.IsPresent() && op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRMoveOp *op) {
    return !op->dst.var.IsPresent() && op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRMoveNegatedOp *op) {
    return !op->dst.var.IsPresent() && op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRSaturatingAddOp *op) {
    return !op->dst.var.IsPresent() && op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRSaturatingSubtractOp *op) {
    return !op->dst.var.IsPresent() && op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRMultiplyOp *op) {
    return !op->dst.var.IsPresent() && op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRMultiplyLongOp *op) {
    return !op->dstLo.var.IsPresent() && !op->dstHi.var.IsPresent() && op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRAddLongOp *op) {
    return !op->dstLo.var.IsPresent() && !op->dstHi.var.IsPresent() && op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRStoreFlagsOp *op) {
    return op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRLoadFlagsOp *op) {
    return !op->dstCPSR.var.IsPresent();
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRLoadStickyOverflowOp *op) {
    return !op->dstCPSR.var.IsPresent();
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRLoadCopRegisterOp *op) {
    return !op->dstValue.var.IsPresent();
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRConstantOp *op) {
    return !op->dst.var.IsPresent();
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRCopyVarOp *op) {
    return !op->dst.var.IsPresent();
}

bool DeadStoreEliminationOptimizerPassBase::IsDeadInstruction(IRGetBaseVectorAddressOp *op) {
    return !op->dst.var.IsPresent();
}

} // namespace armajitto::ir
