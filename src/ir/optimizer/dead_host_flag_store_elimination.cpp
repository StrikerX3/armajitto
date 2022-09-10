#include "dead_host_flag_store_elimination.hpp"

#include "ir/ops/ir_ops_visitor.hpp"

#include <cassert>

namespace armajitto::ir {

void DeadHostFlagStoreEliminationOptimizerPass::Reset() {
    m_writtenFlags = arm::Flags::None;
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRLogicalShiftLeftOp *op) {
    if (op->setCarry) {
        RecordHostFlagsWrite(arm::Flags::C, op);
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    if (op->setCarry) {
        RecordHostFlagsWrite(arm::Flags::C, op);
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    if (op->setCarry) {
        RecordHostFlagsWrite(arm::Flags::C, op);
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRRotateRightOp *op) {
    if (op->setCarry) {
        RecordHostFlagsWrite(arm::Flags::C, op);
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRRotateRightExtendedOp *op) {
    if (op->setCarry) {
        RecordHostFlagsWrite(arm::Flags::C, op);
    }
    RecordHostFlagsRead(arm::Flags::C, op);
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRBitwiseAndOp *op) {
    if (op->flags != arm::Flags::None) {
        RecordHostFlagsWrite(op->kAffectedFlags, op);
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRBitwiseOrOp *op) {
    if (op->flags != arm::Flags::None) {
        RecordHostFlagsWrite(op->kAffectedFlags, op);
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRBitwiseXorOp *op) {
    if (op->flags != arm::Flags::None) {
        RecordHostFlagsWrite(op->kAffectedFlags, op);
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRBitClearOp *op) {
    if (op->flags != arm::Flags::None) {
        RecordHostFlagsWrite(op->kAffectedFlags, op);
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRAddOp *op) {
    if (op->flags != arm::Flags::None) {
        RecordHostFlagsWrite(op->kAffectedFlags, op);
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRAddCarryOp *op) {
    if (op->flags != arm::Flags::None) {
        RecordHostFlagsWrite(op->kAffectedFlags, op);
    }
    RecordHostFlagsRead(arm::Flags::C, op);
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRSubtractOp *op) {
    if (op->flags != arm::Flags::None) {
        RecordHostFlagsWrite(op->kAffectedFlags, op);
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRSubtractCarryOp *op) {
    if (op->flags != arm::Flags::None) {
        RecordHostFlagsWrite(op->kAffectedFlags, op);
    }
    RecordHostFlagsRead(arm::Flags::C, op);
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRMoveOp *op) {
    if (op->flags != arm::Flags::None) {
        RecordHostFlagsWrite(op->kAffectedFlags, op);
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRMoveNegatedOp *op) {
    if (op->flags != arm::Flags::None) {
        RecordHostFlagsWrite(op->kAffectedFlags, op);
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRSaturatingAddOp *op) {
    if (op->flags != arm::Flags::None) {
        RecordHostFlagsWrite(op->kAffectedFlags, op);
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    if (op->flags != arm::Flags::None) {
        RecordHostFlagsWrite(op->kAffectedFlags, op);
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRMultiplyOp *op) {
    if (op->flags != arm::Flags::None) {
        RecordHostFlagsWrite(op->kAffectedFlags, op);
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRMultiplyLongOp *op) {
    if (op->flags != arm::Flags::None) {
        RecordHostFlagsWrite(op->kAffectedFlags, op);
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRAddLongOp *op) {
    if (op->flags != arm::Flags::None) {
        RecordHostFlagsWrite(op->kAffectedFlags, op);
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRStoreFlagsOp *op) {
    RecordHostFlagsWrite(op->flags, op);
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRLoadFlagsOp *op) {
    RecordHostFlagsRead(op->flags, op);
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRLoadStickyOverflowOp *op) {
    if (op->setQ) {
        RecordHostFlagsRead(arm::Flags::V, op);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Host flag writes tracking

void DeadHostFlagStoreEliminationOptimizerPass::RecordHostFlagsRead(arm::Flags flags, IROp *op) {
    bool dead = VisitIROp(op, [this](auto op) -> bool { return IsDeadInstruction(op); });
    if (!dead) {
        m_writtenFlags &= ~flags;
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::RecordHostFlagsWrite(arm::Flags flags, IROp *op) {
    bool dead = VisitIROp(op, [this](auto op) -> bool {
        EraseHostFlagsWrite(m_writtenFlags, op);
        return IsDeadInstruction(op);
    });
    if (!dead) {
        m_writtenFlags |= flags;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Generic EraseHostFlagsWrite

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagsWrite(arm::Flags flags, IRLogicalShiftLeftOp *op) {
    if (BitmaskEnum(flags).AnyOf(arm::Flags::C)) {
        MarkDirty();
        op->setCarry = false;
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagsWrite(arm::Flags flags, IRLogicalShiftRightOp *op) {
    if (BitmaskEnum(flags).AnyOf(arm::Flags::C)) {
        MarkDirty();
        op->setCarry = false;
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagsWrite(arm::Flags flags, IRArithmeticShiftRightOp *op) {
    if (BitmaskEnum(flags).AnyOf(arm::Flags::C)) {
        MarkDirty();
        op->setCarry = false;
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagsWrite(arm::Flags flags, IRRotateRightOp *op) {
    if (BitmaskEnum(flags).AnyOf(arm::Flags::C)) {
        MarkDirty();
        op->setCarry = false;
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagsWrite(arm::Flags flags, IRRotateRightExtendedOp *op) {
    if (BitmaskEnum(flags).AnyOf(arm::Flags::C)) {
        MarkDirty();
        op->setCarry = false;
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagsWrite(arm::Flags flags, IRBitwiseAndOp *op) {
    MarkDirty((op->flags & flags) != arm::Flags::None);
    op->flags &= ~flags;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagsWrite(arm::Flags flags, IRBitwiseOrOp *op) {
    MarkDirty((op->flags & flags) != arm::Flags::None);
    op->flags &= ~flags;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagsWrite(arm::Flags flags, IRBitwiseXorOp *op) {
    MarkDirty((op->flags & flags) != arm::Flags::None);
    op->flags &= ~flags;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagsWrite(arm::Flags flags, IRBitClearOp *op) {
    MarkDirty((op->flags & flags) != arm::Flags::None);
    op->flags &= ~flags;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagsWrite(arm::Flags flags, IRAddOp *op) {
    MarkDirty((op->flags & flags) != arm::Flags::None);
    op->flags &= ~flags;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagsWrite(arm::Flags flags, IRAddCarryOp *op) {
    MarkDirty((op->flags & flags) != arm::Flags::None);
    op->flags &= ~flags;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagsWrite(arm::Flags flags, IRSubtractOp *op) {
    MarkDirty((op->flags & flags) != arm::Flags::None);
    op->flags &= ~flags;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagsWrite(arm::Flags flags, IRSubtractCarryOp *op) {
    MarkDirty((op->flags & flags) != arm::Flags::None);
    op->flags &= ~flags;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagsWrite(arm::Flags flags, IRMoveOp *op) {
    MarkDirty((op->flags & flags) != arm::Flags::None);
    op->flags &= ~flags;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagsWrite(arm::Flags flags, IRMoveNegatedOp *op) {
    MarkDirty((op->flags & flags) != arm::Flags::None);
    op->flags &= ~flags;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagsWrite(arm::Flags flags, IRSaturatingAddOp *op) {
    MarkDirty((op->flags & flags) != arm::Flags::None);
    op->flags &= ~flags;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagsWrite(arm::Flags flags, IRSaturatingSubtractOp *op) {
    MarkDirty((op->flags & flags) != arm::Flags::None);
    op->flags &= ~flags;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagsWrite(arm::Flags flags, IRMultiplyOp *op) {
    MarkDirty((op->flags & flags) != arm::Flags::None);
    op->flags &= ~flags;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagsWrite(arm::Flags flags, IRMultiplyLongOp *op) {
    MarkDirty((op->flags & flags) != arm::Flags::None);
    op->flags &= ~flags;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagsWrite(arm::Flags flags, IRAddLongOp *op) {
    MarkDirty((op->flags & flags) != arm::Flags::None);
    op->flags &= ~flags;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagsWrite(arm::Flags flags, IRStoreFlagsOp *op) {
    MarkDirty((op->flags & flags) != arm::Flags::None);
    op->flags &= ~flags;
    if (op->values.immediate) {
        op->values.imm.value &= ~static_cast<uint32_t>(flags);
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagsWrite(arm::Flags flags, IRLoadFlagsOp *op) {
    MarkDirty((op->flags & flags) != arm::Flags::None);
    op->flags &= ~flags;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagsWrite(arm::Flags flags, IRLoadStickyOverflowOp *op) {
    if (op->setQ && BitmaskEnum(flags).AnyOf(arm::Flags::V)) {
        op->setQ = false;
        MarkDirty();
    }
}

} // namespace armajitto::ir
