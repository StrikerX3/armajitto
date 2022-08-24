#include "dead_flag_value_store_elimination.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

#include <cassert>

namespace armajitto::ir {

DeadFlagValueStoreEliminationOptimizerPass::DeadFlagValueStoreEliminationOptimizerPass(Emitter &emitter)
    : DeadStoreEliminationOptimizerPassBase(emitter) {

    const uint32_t varCount = emitter.VariableCount();
    m_flagWritesPerVar.resize(varCount);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRGetCPSROp *op) {
    InitFlagWrites(op->dst);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRBitwiseAndOp *op) {
    if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
        auto [imm, var] = *split;
        RecordFlagWrites(op->dst, var, static_cast<arm::Flags>(imm), op);
    }
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRBitwiseOrOp *op) {
    if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
        auto [imm, var] = *split;
        RecordFlagWrites(op->dst, var, static_cast<arm::Flags>(imm), op);
    }
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRBitClearOp *op) {
    if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
        auto [imm, var] = *split;
        RecordFlagWrites(op->dst, var, static_cast<arm::Flags>(imm), op);
    }
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRLoadFlagsOp *op) {
    if (!op->srcCPSR.immediate) {
        RecordFlagWrites(op->dstCPSR, op->srcCPSR.var, op->flags, op);
    }
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRLoadStickyOverflowOp *op) {
    if (!op->srcCPSR.immediate && op->setQ) {
        RecordFlagWrites(op->dstCPSR, op->srcCPSR.var, arm::Flags::Q, op);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Flags tracking

void DeadFlagValueStoreEliminationOptimizerPass::ResizeFlagWritesPerVar(size_t index) {
    if (m_flagWritesPerVar.size() <= index) {
        m_flagWritesPerVar.resize(index + 1);
    }
}

void DeadFlagValueStoreEliminationOptimizerPass::InitFlagWrites(VariableArg base) {
    if (!base.var.IsPresent()) {
        return;
    }

    const auto varIndex = base.var.Index();
    ResizeFlagWritesPerVar(varIndex);
    m_flagWritesPerVar[varIndex].base = base.var;
}

void DeadFlagValueStoreEliminationOptimizerPass::RecordFlagWrites(VariableArg dst, VariableArg src, arm::Flags flags,
                                                                  IROp *writerOp) {
    if (!dst.var.IsPresent() || !src.var.IsPresent()) {
        return;
    }
    if (flags == arm::Flags::None) {
        return;
    }

    const auto dstIndex = dst.var.Index();
    const auto srcIndex = src.var.Index();
    if (srcIndex >= m_flagWritesPerVar.size()) {
        return;
    }
    ResizeFlagWritesPerVar(dstIndex);

    const auto bmFlags = BitmaskEnum(flags);
    auto &srcEntry = m_flagWritesPerVar[srcIndex];
    auto &dstEntry = m_flagWritesPerVar[dstIndex];
    dstEntry = srcEntry;

    auto updateWrite = [&](arm::Flags flag, IROp *&srcOp, IROp *&dstOp) {
        if (bmFlags.AllOf(flag)) {
            if (srcOp != nullptr) {
                VisitIROp(srcOp, [this, flag](auto *op) { EraseFlagWrite(flag, op); });
            }
            dstOp = writerOp;
        }
    };
    updateWrite(arm::Flags::N, srcEntry.writerOpN, dstEntry.writerOpN);
    updateWrite(arm::Flags::Z, srcEntry.writerOpZ, dstEntry.writerOpZ);
    updateWrite(arm::Flags::C, srcEntry.writerOpC, dstEntry.writerOpC);
    updateWrite(arm::Flags::V, srcEntry.writerOpV, dstEntry.writerOpV);
    updateWrite(arm::Flags::Q, srcEntry.writerOpQ, dstEntry.writerOpQ);
}

// ---------------------------------------------------------------------------------------------------------------------
// Generic EraseFlagWrite

void DeadFlagValueStoreEliminationOptimizerPass::EraseFlagWrite(arm::Flags flag, IRBitwiseAndOp *op) {
    if (auto split = SplitImmVarArgPair(op->lhs, op->rhs)) {
        auto &[imm, _] = *split;
        MarkDirty(imm.value & ~static_cast<uint32_t>(flag));
        imm.value |= static_cast<uint32_t>(flag);
    }
}

void DeadFlagValueStoreEliminationOptimizerPass::EraseFlagWrite(arm::Flags flag, IRBitwiseOrOp *op) {
    if (auto split = SplitImmVarArgPair(op->lhs, op->rhs)) {
        auto &[imm, _] = *split;
        MarkDirty(imm.value & static_cast<uint32_t>(flag));
        imm.value &= ~static_cast<uint32_t>(flag);
    }
}

void DeadFlagValueStoreEliminationOptimizerPass::EraseFlagWrite(arm::Flags flag, IRBitClearOp *op) {
    if (auto split = SplitImmVarArgPair(op->lhs, op->rhs)) {
        auto &[imm, _] = *split;
        MarkDirty(imm.value & static_cast<uint32_t>(flag));
        imm.value &= ~static_cast<uint32_t>(flag);
    }
}

void DeadFlagValueStoreEliminationOptimizerPass::EraseFlagWrite(arm::Flags flag, IRLoadFlagsOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
}

void DeadFlagValueStoreEliminationOptimizerPass::EraseFlagWrite(arm::Flags flag, IRLoadStickyOverflowOp *op) {
    if (op->setQ && BitmaskEnum(flag).AnyOf(arm::Flags::Q)) {
        op->setQ = false;
        MarkDirty();
    }
}

} // namespace armajitto::ir
