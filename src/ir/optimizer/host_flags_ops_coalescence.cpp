#include "host_flags_ops_coalescence.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

namespace armajitto::ir {

HostFlagsOpsCoalescenceOptimizerPass::HostFlagsOpsCoalescenceOptimizerPass(Emitter &emitter)
    : OptimizerPassBase(emitter) {}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRLogicalShiftLeftOp *op) {
    if (m_storeFlagsOp != nullptr && op->setCarry && BitmaskEnum(m_storeFlagsOp->flags).AnyOf(arm::Flags::C)) {
        m_storeFlagsOp = nullptr;
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    if (m_storeFlagsOp != nullptr && op->setCarry && BitmaskEnum(m_storeFlagsOp->flags).AnyOf(arm::Flags::C)) {
        m_storeFlagsOp = nullptr;
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    if (m_storeFlagsOp != nullptr && op->setCarry && BitmaskEnum(m_storeFlagsOp->flags).AnyOf(arm::Flags::C)) {
        m_storeFlagsOp = nullptr;
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRRotateRightOp *op) {
    if (m_storeFlagsOp != nullptr && op->setCarry && BitmaskEnum(m_storeFlagsOp->flags).AnyOf(arm::Flags::C)) {
        m_storeFlagsOp = nullptr;
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRRotateRightExtendedOp *op) {
    if (m_storeFlagsOp != nullptr && BitmaskEnum(m_storeFlagsOp->flags).AnyOf(arm::Flags::C)) {
        m_storeFlagsOp = nullptr;
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRBitwiseAndOp *op) {
    if (m_storeFlagsOp != nullptr && BitmaskEnum(m_storeFlagsOp->flags).AnyOf(op->flags)) {
        m_storeFlagsOp = nullptr;
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRBitwiseOrOp *op) {
    if (m_storeFlagsOp != nullptr && BitmaskEnum(m_storeFlagsOp->flags).AnyOf(op->flags)) {
        m_storeFlagsOp = nullptr;
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRBitwiseXorOp *op) {
    if (m_storeFlagsOp != nullptr && BitmaskEnum(m_storeFlagsOp->flags).AnyOf(op->flags)) {
        m_storeFlagsOp = nullptr;
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRBitClearOp *op) {
    if (m_storeFlagsOp != nullptr && BitmaskEnum(m_storeFlagsOp->flags).AnyOf(op->flags)) {
        m_storeFlagsOp = nullptr;
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRAddOp *op) {
    if (m_storeFlagsOp != nullptr && BitmaskEnum(m_storeFlagsOp->flags).AnyOf(op->flags)) {
        m_storeFlagsOp = nullptr;
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRAddCarryOp *op) {
    if (m_storeFlagsOp != nullptr && BitmaskEnum(m_storeFlagsOp->flags).AnyOf(op->flags | arm::Flags::C)) {
        m_storeFlagsOp = nullptr;
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRSubtractOp *op) {
    if (m_storeFlagsOp != nullptr && BitmaskEnum(m_storeFlagsOp->flags).AnyOf(op->flags)) {
        m_storeFlagsOp = nullptr;
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRSubtractCarryOp *op) {
    if (m_storeFlagsOp != nullptr && BitmaskEnum(m_storeFlagsOp->flags).AnyOf(op->flags | arm::Flags::C)) {
        m_storeFlagsOp = nullptr;
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRMoveOp *op) {
    if (m_storeFlagsOp != nullptr && BitmaskEnum(m_storeFlagsOp->flags).AnyOf(op->flags)) {
        m_storeFlagsOp = nullptr;
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRMoveNegatedOp *op) {
    if (m_storeFlagsOp != nullptr && BitmaskEnum(m_storeFlagsOp->flags).AnyOf(op->flags)) {
        m_storeFlagsOp = nullptr;
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRSaturatingAddOp *op) {
    if (m_storeFlagsOp != nullptr && BitmaskEnum(m_storeFlagsOp->flags).AnyOf(op->flags | arm::Flags::Q)) {
        m_storeFlagsOp = nullptr;
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    if (m_storeFlagsOp != nullptr && BitmaskEnum(m_storeFlagsOp->flags).AnyOf(op->flags | arm::Flags::Q)) {
        m_storeFlagsOp = nullptr;
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRMultiplyOp *op) {
    if (m_storeFlagsOp != nullptr && BitmaskEnum(m_storeFlagsOp->flags).AnyOf(op->flags)) {
        m_storeFlagsOp = nullptr;
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRMultiplyLongOp *op) {
    if (m_storeFlagsOp != nullptr && BitmaskEnum(m_storeFlagsOp->flags).AnyOf(op->flags)) {
        m_storeFlagsOp = nullptr;
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRAddLongOp *op) {
    if (m_storeFlagsOp != nullptr && BitmaskEnum(m_storeFlagsOp->flags).AnyOf(op->flags)) {
        m_storeFlagsOp = nullptr;
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRStoreFlagsOp *op) {
    // Merge flags into previously known instruction, otherwise point to this instruction if the values are unknown or
    // this is the first StoreFlags instruction in the sequence
    if (m_storeFlagsOp != nullptr && op->values.immediate) {
        m_storeFlagsOp->flags |= op->flags;
        m_storeFlagsOp->values.imm.value &= ~static_cast<uint32_t>(op->flags);
        m_storeFlagsOp->values.imm.value |= op->values.imm.value & static_cast<uint32_t>(op->flags);
        m_emitter.Erase(op);
    } else {
        m_storeFlagsOp = op;
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRLoadFlagsOp *op) {
    if (m_storeFlagsOp != nullptr && BitmaskEnum(m_storeFlagsOp->flags).AnyOf(op->flags)) {
        m_storeFlagsOp = nullptr;
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRLoadStickyOverflowOp *op) {
    if (m_storeFlagsOp != nullptr && BitmaskEnum(m_storeFlagsOp->flags).AnyOf(arm::Flags::Q)) {
        m_storeFlagsOp = nullptr;
    }
}

} // namespace armajitto::ir
