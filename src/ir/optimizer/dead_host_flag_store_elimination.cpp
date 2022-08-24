#include "dead_host_flag_store_elimination.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

#include <cassert>

namespace armajitto::ir {

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
    RecordHostFlagsRead(arm::Flags::C);
    if (op->setCarry) {
        RecordHostFlagsWrite(arm::Flags::C, op);
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRBitwiseAndOp *op) {
    RecordHostFlagsWrite(op->flags, op);
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRBitwiseOrOp *op) {
    RecordHostFlagsWrite(op->flags, op);
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRBitwiseXorOp *op) {
    RecordHostFlagsWrite(op->flags, op);
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRBitClearOp *op) {
    RecordHostFlagsWrite(op->flags, op);
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRAddOp *op) {
    RecordHostFlagsWrite(op->flags, op);
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRAddCarryOp *op) {
    RecordHostFlagsRead(arm::Flags::C);
    RecordHostFlagsWrite(op->flags, op);
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRSubtractOp *op) {
    RecordHostFlagsWrite(op->flags, op);
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRSubtractCarryOp *op) {
    RecordHostFlagsRead(arm::Flags::C);
    RecordHostFlagsWrite(op->flags, op);
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRMoveOp *op) {
    RecordHostFlagsWrite(op->flags, op);
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRMoveNegatedOp *op) {
    RecordHostFlagsWrite(op->flags, op);
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRSaturatingAddOp *op) {
    RecordHostFlagsWrite(op->flags, op);
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    RecordHostFlagsWrite(op->flags, op);
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRMultiplyOp *op) {
    RecordHostFlagsWrite(op->flags, op);
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRMultiplyLongOp *op) {
    RecordHostFlagsWrite(op->flags, op);
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRAddLongOp *op) {
    RecordHostFlagsWrite(op->flags, op);
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRStoreFlagsOp *op) {
    RecordHostFlagsWrite(op->flags, op);
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRLoadFlagsOp *op) {
    RecordHostFlagsRead(op->flags);
}

void DeadHostFlagStoreEliminationOptimizerPass::Process(IRLoadStickyOverflowOp *op) {
    if (op->setQ) {
        RecordHostFlagsRead(arm::Flags::Q);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Host flag writes tracking

void DeadHostFlagStoreEliminationOptimizerPass::RecordHostFlagsRead(arm::Flags flags) {
    auto bmFlags = BitmaskEnum(flags);
    auto record = [&](arm::Flags flag, IROp *&write) {
        if (bmFlags.AnyOf(flag)) {
            write = nullptr;
        }
    };
    record(arm::Flags::N, m_hostFlagWriteN);
    record(arm::Flags::Z, m_hostFlagWriteZ);
    record(arm::Flags::C, m_hostFlagWriteC);
    record(arm::Flags::V, m_hostFlagWriteV);
    record(arm::Flags::Q, m_hostFlagWriteQ);
}

void DeadHostFlagStoreEliminationOptimizerPass::RecordHostFlagsWrite(arm::Flags flags, IROp *op) {
    auto bmFlags = BitmaskEnum(flags);
    if (bmFlags.None()) {
        return;
    }
    auto record = [&](arm::Flags flag, IROp *&write) {
        if (bmFlags.AnyOf(flag)) {
            if (write != nullptr) {
                VisitIROp(write, [this, flag](auto op) -> void { EraseHostFlagWrite(flag, op); });
            }
            write = op;
        }
    };
    record(arm::Flags::N, m_hostFlagWriteN);
    record(arm::Flags::Z, m_hostFlagWriteZ);
    record(arm::Flags::C, m_hostFlagWriteC);
    record(arm::Flags::V, m_hostFlagWriteV);
    record(arm::Flags::Q, m_hostFlagWriteQ);
}

// ---------------------------------------------------------------------------------------------------------------------
// Generic EraseHostFlagWrite

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRLogicalShiftLeftOp *op) {
    if (BitmaskEnum(flag).AnyOf(arm::Flags::C)) {
        MarkDirty();
        op->setCarry = false;
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRLogicalShiftRightOp *op) {
    if (BitmaskEnum(flag).AnyOf(arm::Flags::C)) {
        MarkDirty();
        op->setCarry = false;
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRArithmeticShiftRightOp *op) {
    if (BitmaskEnum(flag).AnyOf(arm::Flags::C)) {
        MarkDirty();
        op->setCarry = false;
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRRotateRightOp *op) {
    if (BitmaskEnum(flag).AnyOf(arm::Flags::C)) {
        MarkDirty();
        op->setCarry = false;
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRRotateRightExtendedOp *op) {
    if (BitmaskEnum(flag).AnyOf(arm::Flags::C)) {
        MarkDirty();
        op->setCarry = false;
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRBitwiseAndOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRBitwiseOrOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRBitwiseXorOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRBitClearOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRAddOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRAddCarryOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRSubtractOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRSubtractCarryOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRMoveOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRMoveNegatedOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRSaturatingAddOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRSaturatingSubtractOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRMultiplyOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRMultiplyLongOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRAddLongOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRStoreFlagsOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
    if (op->values.immediate) {
        op->values.imm.value &= ~static_cast<uint32_t>(flag);
    }
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRLoadFlagsOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
}

void DeadHostFlagStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRLoadStickyOverflowOp *op) {
    if (op->setQ && BitmaskEnum(flag).AnyOf(arm::Flags::Q)) {
        op->setQ = false;
        MarkDirty();
    }
}

} // namespace armajitto::ir
