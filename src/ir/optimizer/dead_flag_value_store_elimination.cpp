#include "dead_flag_value_store_elimination.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

#include <cassert>

namespace armajitto::ir {

DeadFlagValueStoreEliminationOptimizerPass::DeadFlagValueStoreEliminationOptimizerPass(Emitter &emitter,
                                                                                       std::pmr::memory_resource &alloc)
    : DeadStoreEliminationOptimizerPassBase(emitter)
    , m_flagWritesPerVar(&alloc) {

    const uint32_t varCount = emitter.VariableCount();
    m_flagWritesPerVar.resize(varCount);
}

void DeadFlagValueStoreEliminationOptimizerPass::Reset() {
    std::fill(m_flagWritesPerVar.begin(), m_flagWritesPerVar.end(), FlagWrites{});
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRSetRegisterOp *op) {
    ConsumeFlags(op->src);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRGetCPSROp *op) {
    InitFlagWrites(op->dst);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRSetCPSROp *op) {
    ConsumeFlags(op->src);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRSetSPSROp *op) {
    ConsumeFlags(op->src);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRMemReadOp *op) {
    ConsumeFlags(op->address);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRMemWriteOp *op) {
    ConsumeFlags(op->src);
    ConsumeFlags(op->address);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRPreloadOp *op) {
    ConsumeFlags(op->address);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRLogicalShiftLeftOp *op) {
    ConsumeFlags(op->value);
    ConsumeFlags(op->amount);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    ConsumeFlags(op->value);
    ConsumeFlags(op->amount);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    ConsumeFlags(op->value);
    ConsumeFlags(op->amount);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRRotateRightOp *op) {
    ConsumeFlags(op->value);
    ConsumeFlags(op->amount);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRRotateRightExtendedOp *op) {
    ConsumeFlags(op->value);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRBitwiseAndOp *op) {
    if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
        auto [imm, var] = *split;
        RecordFlagWrites(op->dst, var, static_cast<arm::Flags>(imm), op);
    } else {
        ConsumeFlags(op->lhs);
        ConsumeFlags(op->rhs);
    }
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRBitwiseOrOp *op) {
    if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
        auto [imm, var] = *split;
        RecordFlagWrites(op->dst, var, static_cast<arm::Flags>(imm), op);
    } else {
        ConsumeFlags(op->lhs);
        ConsumeFlags(op->rhs);
    }
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRBitwiseXorOp *op) {
    ConsumeFlags(op->lhs);
    ConsumeFlags(op->rhs);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRBitClearOp *op) {
    if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
        auto [imm, var] = *split;
        RecordFlagWrites(op->dst, var, static_cast<arm::Flags>(imm), op);
    } else {
        ConsumeFlags(op->lhs);
        ConsumeFlags(op->rhs);
    }
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRCountLeadingZerosOp *op) {
    ConsumeFlags(op->value);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRAddOp *op) {
    ConsumeFlags(op->lhs);
    ConsumeFlags(op->rhs);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRAddCarryOp *op) {
    ConsumeFlags(op->lhs);
    ConsumeFlags(op->rhs);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRSubtractOp *op) {
    ConsumeFlags(op->lhs);
    ConsumeFlags(op->rhs);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRSubtractCarryOp *op) {
    ConsumeFlags(op->lhs);
    ConsumeFlags(op->rhs);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRMoveOp *op) {
    ConsumeFlags(op->value);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRMoveNegatedOp *op) {
    ConsumeFlags(op->value);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRSaturatingAddOp *op) {
    ConsumeFlags(op->lhs);
    ConsumeFlags(op->rhs);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    ConsumeFlags(op->lhs);
    ConsumeFlags(op->rhs);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRMultiplyOp *op) {
    ConsumeFlags(op->lhs);
    ConsumeFlags(op->rhs);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRMultiplyLongOp *op) {
    ConsumeFlags(op->lhs);
    ConsumeFlags(op->rhs);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRAddLongOp *op) {
    ConsumeFlags(op->lhsLo);
    ConsumeFlags(op->lhsHi);
    ConsumeFlags(op->rhsLo);
    ConsumeFlags(op->rhsHi);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRStoreFlagsOp *op) {
    ConsumeFlags(op->values);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRLoadFlagsOp *op) {
    if (!op->srcCPSR.immediate) {
        RecordFlagWrites(op->dstCPSR, op->srcCPSR.var, op->flags, op);
    }
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRBranchOp *op) {
    ConsumeFlags(op->address);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRBranchExchangeOp *op) {
    ConsumeFlags(op->address);
}

void DeadFlagValueStoreEliminationOptimizerPass::Process(IRStoreCopRegisterOp *op) {
    ConsumeFlags(op->srcValue);
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
}

void DeadFlagValueStoreEliminationOptimizerPass::ConsumeFlags(VariableArg &arg) {
    if (!arg.var.IsPresent()) {
        return;
    }
    const auto varIndex = arg.var.Index();
    if (varIndex >= m_flagWritesPerVar.size()) {
        return;
    }
    m_flagWritesPerVar[varIndex].writerOpN = nullptr;
    m_flagWritesPerVar[varIndex].writerOpZ = nullptr;
    m_flagWritesPerVar[varIndex].writerOpC = nullptr;
    m_flagWritesPerVar[varIndex].writerOpV = nullptr;
}

void DeadFlagValueStoreEliminationOptimizerPass::ConsumeFlags(VarOrImmArg &arg) {
    if (!arg.immediate) {
        ConsumeFlags(arg.var);
    }
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
    if (op->setQ && BitmaskEnum(flag).AnyOf(arm::Flags::V)) {
        op->setQ = false;
        MarkDirty();
    }
}

} // namespace armajitto::ir
