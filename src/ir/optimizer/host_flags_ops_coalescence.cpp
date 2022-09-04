#include "host_flags_ops_coalescence.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

namespace armajitto::ir {

HostFlagsOpsCoalescenceOptimizerPass::HostFlagsOpsCoalescenceOptimizerPass(Emitter &emitter)
    : OptimizerPassBase(emitter) {}

void HostFlagsOpsCoalescenceOptimizerPass::Reset() {
    m_storeFlagsOp = nullptr;
    m_loadFlagsOpN = nullptr;
    m_loadFlagsOpZ = nullptr;
    m_loadFlagsOpC = nullptr;
    m_loadFlagsOpV = nullptr;
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRLogicalShiftLeftOp *op) {
    if (op->setCarry) {
        UpdateFlags(arm::Flags::C);
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    if (op->setCarry) {
        UpdateFlags(arm::Flags::C);
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    if (op->setCarry) {
        UpdateFlags(arm::Flags::C);
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRRotateRightOp *op) {
    if (op->setCarry) {
        UpdateFlags(arm::Flags::C);
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRRotateRightExtendedOp *op) {
    ConsumeFlags(arm::Flags::C);
    if (op->setCarry) {
        UpdateFlags(arm::Flags::C);
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRBitwiseAndOp *op) {
    UpdateFlags(op->flags);
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRBitwiseOrOp *op) {
    UpdateFlags(op->flags);
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRBitwiseXorOp *op) {
    UpdateFlags(op->flags);
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRBitClearOp *op) {
    UpdateFlags(op->flags);
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRAddOp *op) {
    UpdateFlags(op->flags);
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRAddCarryOp *op) {
    ConsumeFlags(arm::Flags::C);
    UpdateFlags(op->flags);
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRSubtractOp *op) {
    UpdateFlags(op->flags);
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRSubtractCarryOp *op) {
    ConsumeFlags(arm::Flags::C);
    UpdateFlags(op->flags);
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRMoveOp *op) {
    UpdateFlags(op->flags);
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRMoveNegatedOp *op) {
    UpdateFlags(op->flags);
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRSaturatingAddOp *op) {
    ConsumeFlags(arm::Flags::V);
    UpdateFlags(op->flags);
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    ConsumeFlags(arm::Flags::V);
    UpdateFlags(op->flags);
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRMultiplyOp *op) {
    UpdateFlags(op->flags);
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRMultiplyLongOp *op) {
    UpdateFlags(op->flags);
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRAddLongOp *op) {
    UpdateFlags(op->flags);
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
    // Merge previous flags into this instruction, otherwise point to this instruction
    auto bmFlags = BitmaskEnum(op->flags);
    auto update = [this, &bmFlags, &op](arm::Flags flag, IRLoadFlagsOp *&loadOp) {
        // If another ldflg instruction wrote to this flag, move the flag update to the current instruction
        if (loadOp != nullptr) {
            // Only if the two instruction are in the same chain of operations
            if (loadOp->dstCPSR == op->srcCPSR) {
                op->flags |= flag;
                loadOp->flags &= ~flag;

                // If the previous load instruction no longer updates any flags, erase it and repoint the current
                // instruction's source CPSR to the previous instruction's
                if (loadOp->flags == arm::Flags::None) {
                    op->srcCPSR = loadOp->srcCPSR;
                    m_emitter.Erase(loadOp);
                }
            }
        } else if (bmFlags.AnyOf(flag)) {
            // Track new write to the flag
            loadOp = op;
        }
    };
    update(arm::Flags::N, m_loadFlagsOpN);
    update(arm::Flags::Z, m_loadFlagsOpZ);
    update(arm::Flags::C, m_loadFlagsOpC);
    update(arm::Flags::V, m_loadFlagsOpV);

    ConsumeFlags(op->flags);
}

void HostFlagsOpsCoalescenceOptimizerPass::Process(IRLoadStickyOverflowOp *op) {
    ConsumeFlags(arm::Flags::V);
}

// ---------------------------------------------------------------------------------------------------------------------

void HostFlagsOpsCoalescenceOptimizerPass::UpdateFlags(arm::Flags flags) {
    auto bmFlags = BitmaskEnum(flags);
    if (bmFlags.Any()) {
        m_storeFlagsOp = nullptr;
    }

    if (bmFlags.AllOf(arm::Flags::N)) {
        m_loadFlagsOpN = nullptr;
    }
    if (bmFlags.AllOf(arm::Flags::Z)) {
        m_loadFlagsOpZ = nullptr;
    }
    if (bmFlags.AllOf(arm::Flags::C)) {
        m_loadFlagsOpC = nullptr;
    }
    if (bmFlags.AllOf(arm::Flags::V)) {
        m_loadFlagsOpV = nullptr;
    }
}

void HostFlagsOpsCoalescenceOptimizerPass::ConsumeFlags(arm::Flags flags) {
    if (m_storeFlagsOp != nullptr && BitmaskEnum(m_storeFlagsOp->flags).AnyOf(flags)) {
        m_storeFlagsOp = nullptr;
    }
}

} // namespace armajitto::ir
