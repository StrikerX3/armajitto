#include "dead_store_elimination_base.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

#include <cassert>

namespace armajitto::ir {

DeadStoreEliminationOptimizerPassBase::DeadStoreEliminationOptimizerPassBase(Emitter &emitter)
    : OptimizerPassBase(emitter) {}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRGetRegisterOp *op) {
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRGetCPSROp *op) {
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRGetSPSROp *op) {
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRMemReadOp *op) {
    if (!op->dst.var.IsPresent()) {
        if (op->address.immediate && false /* TODO: no side effects on address */) {
            m_emitter.Erase(op);
            return true;
        }
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRLogicalShiftLeftOp *op) {
    if (!op->dst.var.IsPresent() && !op->setCarry) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRLogicalShiftRightOp *op) {
    if (!op->dst.var.IsPresent() && !op->setCarry) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRArithmeticShiftRightOp *op) {
    if (!op->dst.var.IsPresent() && !op->setCarry) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRRotateRightOp *op) {
    if (!op->dst.var.IsPresent() && !op->setCarry) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRRotateRightExtendedOp *op) {
    if (!op->dst.var.IsPresent() && !op->setCarry) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRBitwiseAndOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRBitwiseOrOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRBitwiseXorOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRBitClearOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRCountLeadingZerosOp *op) {
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRAddOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRAddCarryOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRSubtractOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRSubtractCarryOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRMoveOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRMoveNegatedOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRSaturatingAddOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRSaturatingSubtractOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRMultiplyOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRMultiplyLongOp *op) {
    if (!op->dstLo.var.IsPresent() && !op->dstHi.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRAddLongOp *op) {
    if (!op->dstLo.var.IsPresent() && !op->dstHi.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRStoreFlagsOp *op) {
    if (op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRLoadFlagsOp *op) {
    if (!op->dstCPSR.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRLoadStickyOverflowOp *op) {
    if (!op->dstCPSR.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRLoadCopRegisterOp *op) {
    if (!op->dstValue.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRConstantOp *op) {
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRCopyVarOp *op) {
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPassBase::EraseDeadInstruction(IRGetBaseVectorAddressOp *op) {
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

} // namespace armajitto::ir
