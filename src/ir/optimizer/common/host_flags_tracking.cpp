#include "host_flags_tracking.hpp"

#include "ir/ops/ir_ops_visitor.hpp"

namespace armajitto::ir {

void HostFlagStateTracker::Reset() {
    m_known = arm::Flags::None;
    m_state = arm::Flags::None;
}

void HostFlagStateTracker::Update(IROp *op) {
    VisitIROp(op, [this](auto *op) { return UpdateImpl(op); });
}

// ---------------------------------------------------------------------------------------------------------------------

void HostFlagStateTracker::Unknown(arm::Flags flags) {
    m_known &= ~flags;
    m_state &= ~flags;
}

void HostFlagStateTracker::Known(arm::Flags flags, arm::Flags values) {
    m_known |= flags;
    m_state = (m_state & ~flags) | (values & flags);
}

void HostFlagStateTracker::UpdateImpl(IRLogicalShiftLeftOp *op) {
    if (op->setCarry) {
        Unknown(arm::Flags::C);
    }
}

void HostFlagStateTracker::UpdateImpl(IRLogicalShiftRightOp *op) {
    if (op->setCarry) {
        Unknown(arm::Flags::C);
    }
}

void HostFlagStateTracker::UpdateImpl(IRArithmeticShiftRightOp *op) {
    if (op->setCarry) {
        Unknown(arm::Flags::C);
    }
}

void HostFlagStateTracker::UpdateImpl(IRRotateRightOp *op) {
    if (op->setCarry) {
        Unknown(arm::Flags::C);
    }
}

void HostFlagStateTracker::UpdateImpl(IRRotateRightExtendedOp *op) {
    if (op->setCarry) {
        Unknown(arm::Flags::C);
    }
}

void HostFlagStateTracker::UpdateImpl(IRBitwiseAndOp *op) {
    Unknown(op->flags);
}

void HostFlagStateTracker::UpdateImpl(IRBitwiseOrOp *op) {
    Unknown(op->flags);
}

void HostFlagStateTracker::UpdateImpl(IRBitwiseXorOp *op) {
    Unknown(op->flags);
}

void HostFlagStateTracker::UpdateImpl(IRBitClearOp *op) {
    Unknown(op->flags);
}

void HostFlagStateTracker::UpdateImpl(IRAddOp *op) {
    Unknown(op->flags);
}

void HostFlagStateTracker::UpdateImpl(IRAddCarryOp *op) {
    Unknown(op->flags);
}

void HostFlagStateTracker::UpdateImpl(IRSubtractOp *op) {
    Unknown(op->flags);
}

void HostFlagStateTracker::UpdateImpl(IRSubtractCarryOp *op) {
    Unknown(op->flags);
}

void HostFlagStateTracker::UpdateImpl(IRMoveOp *op) {
    Unknown(op->flags);
}

void HostFlagStateTracker::UpdateImpl(IRMoveNegatedOp *op) {
    Unknown(op->flags);
}

void HostFlagStateTracker::UpdateImpl(IRSaturatingAddOp *op) {
    Unknown(op->flags);
}

void HostFlagStateTracker::UpdateImpl(IRSaturatingSubtractOp *op) {
    Unknown(op->flags);
}

void HostFlagStateTracker::UpdateImpl(IRMultiplyOp *op) {
    Unknown(op->flags);
}

void HostFlagStateTracker::UpdateImpl(IRMultiplyLongOp *op) {
    Unknown(op->flags);
}

void HostFlagStateTracker::UpdateImpl(IRAddLongOp *op) {
    Unknown(op->flags);
}

void HostFlagStateTracker::UpdateImpl(IRStoreFlagsOp *op) {
    if (op->values.immediate) {
        Known(op->flags, static_cast<arm::Flags>(op->values.imm.value));
    } else {
        Unknown(op->flags);
    }
}

} // namespace armajitto::ir
