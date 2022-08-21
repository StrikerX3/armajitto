#include "dead_store.hpp"

namespace armajitto::ir {

void DeadStoreEliminationOptimizerPass::PostProcess() {
    for (size_t i = 0; i < m_varWrites.size(); i++) {
        auto *op = m_varWrites[i];
        Variable var{i};
        if (op != nullptr) {
            // TODO: erase write to var in op
            // TODO: erase instruction if it has no more writes or side effects
            // FIXME: this is a HACK to get things going
            m_emitter.Erase(op);
        }
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRGetRegisterOp *op) {
    RecordWrite(op->dst, op);
    // TODO: deal with GPRs
}

void DeadStoreEliminationOptimizerPass::Process(IRSetRegisterOp *op) {
    RecordRead(op->src);
    // TODO: deal with GPRs
}

void DeadStoreEliminationOptimizerPass::Process(IRGetCPSROp *op) {
    RecordWrite(op->dst, op);
    // TODO: deal with PSRs
}

void DeadStoreEliminationOptimizerPass::Process(IRSetCPSROp *op) {
    RecordRead(op->src);
    // TODO: deal with PSRs
}

void DeadStoreEliminationOptimizerPass::Process(IRGetSPSROp *op) {
    RecordWrite(op->dst, op);
    // TODO: deal with PSRs
}

void DeadStoreEliminationOptimizerPass::Process(IRSetSPSROp *op) {
    RecordRead(op->src);
    // TODO: deal with PSRs
}

void DeadStoreEliminationOptimizerPass::Process(IRMemReadOp *op) {
    RecordWrite(op->dst, op);
    RecordDependency(op->dst, op->address);
}

void DeadStoreEliminationOptimizerPass::Process(IRMemWriteOp *op) {
    RecordRead(op->src);
    RecordRead(op->address);
}

void DeadStoreEliminationOptimizerPass::Process(IRPreloadOp *op) {
    RecordRead(op->address);
}

void DeadStoreEliminationOptimizerPass::Process(IRLogicalShiftLeftOp *op) {
    RecordWrite(op->dst, op);
    RecordDependency(op->dst, op->value);
    RecordDependency(op->dst, op->amount);
    if (op->setFlags) {
        // TODO: C flag is written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    RecordWrite(op->dst, op);
    RecordDependency(op->dst, op->value);
    RecordDependency(op->dst, op->amount);
    if (op->setFlags) {
        // TODO: C flag is written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    RecordWrite(op->dst, op);
    RecordDependency(op->dst, op->value);
    RecordDependency(op->dst, op->amount);
    if (op->setFlags) {
        // TODO: C flag is written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRRotateRightOp *op) {
    RecordWrite(op->dst, op);
    RecordDependency(op->dst, op->value);
    RecordDependency(op->dst, op->amount);
    if (op->setFlags) {
        // TODO: C flag is written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRRotateRightExtendOp *op) {
    RecordWrite(op->dst, op);
    RecordDependency(op->dst, op->value);
    // TODO: C flag is read
    if (op->setFlags) {
        // TODO: C flag is written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRBitwiseAndOp *op) {
    RecordWrite(op->dst, op);
    RecordDependency(op->dst, op->lhs);
    RecordDependency(op->dst, op->rhs);
    if (op->setFlags) {
        // TODO: NZ flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRBitwiseOrOp *op) {
    RecordWrite(op->dst, op);
    RecordDependency(op->dst, op->lhs);
    RecordDependency(op->dst, op->rhs);
    if (op->setFlags) {
        // TODO: NZ flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRBitwiseXorOp *op) {
    RecordWrite(op->dst, op);
    RecordDependency(op->dst, op->lhs);
    RecordDependency(op->dst, op->rhs);
    if (op->setFlags) {
        // TODO: NZ flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRBitClearOp *op) {
    RecordWrite(op->dst, op);
    RecordDependency(op->dst, op->lhs);
    RecordDependency(op->dst, op->rhs);
    if (op->setFlags) {
        // TODO: NZ flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRCountLeadingZerosOp *op) {
    RecordWrite(op->dst, op);
    RecordDependency(op->dst, op->value);
}

void DeadStoreEliminationOptimizerPass::Process(IRAddOp *op) {
    RecordWrite(op->dst, op);
    RecordDependency(op->dst, op->lhs);
    RecordDependency(op->dst, op->rhs);
    if (op->setFlags) {
        // TODO: NZCV flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRAddCarryOp *op) {
    RecordWrite(op->dst, op);
    RecordDependency(op->dst, op->lhs);
    RecordDependency(op->dst, op->rhs);
    // TODO: C flag is read
    if (op->setFlags) {
        // TODO: NZCV flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRSubtractOp *op) {
    RecordWrite(op->dst, op);
    RecordDependency(op->dst, op->lhs);
    RecordDependency(op->dst, op->rhs);
    if (op->setFlags) {
        // TODO: NZCV flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRSubtractCarryOp *op) {
    RecordWrite(op->dst, op);
    RecordDependency(op->dst, op->lhs);
    RecordDependency(op->dst, op->rhs);
    // TODO: C flag is read
    if (op->setFlags) {
        // TODO: NZCV flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRMoveOp *op) {
    RecordWrite(op->dst, op);
    RecordDependency(op->dst, op->value);
    if (op->setFlags) {
        // TODO: NZ flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRMoveNegatedOp *op) {
    RecordWrite(op->dst, op);
    RecordDependency(op->dst, op->value);
    if (op->setFlags) {
        // TODO: NZ flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRSaturatingAddOp *op) {
    RecordWrite(op->dst, op);
    RecordDependency(op->dst, op->lhs);
    RecordDependency(op->dst, op->rhs);
    // TODO: Q flag is written
}

void DeadStoreEliminationOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    RecordWrite(op->dst, op);
    RecordDependency(op->dst, op->lhs);
    RecordDependency(op->dst, op->rhs);
    // TODO: Q flag is written
}

void DeadStoreEliminationOptimizerPass::Process(IRMultiplyOp *op) {
    RecordWrite(op->dst, op);
    RecordDependency(op->dst, op->lhs);
    RecordDependency(op->dst, op->rhs);
    if (op->setFlags) {
        // TODO: NZ flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRMultiplyLongOp *op) {
    RecordWrite(op->dstLo, op);
    RecordWrite(op->dstHi, op);
    RecordDependency(op->dstLo, op->lhs);
    RecordDependency(op->dstLo, op->rhs);
    RecordDependency(op->dstHi, op->lhs);
    RecordDependency(op->dstHi, op->rhs);
    if (op->setFlags) {
        // TODO: NZ flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRAddLongOp *op) {
    RecordWrite(op->dstLo, op);
    RecordWrite(op->dstHi, op);
    RecordDependency(op->dstLo, op->lhsLo);
    RecordDependency(op->dstLo, op->lhsHi);
    RecordDependency(op->dstLo, op->rhsLo);
    RecordDependency(op->dstLo, op->rhsHi);
    RecordDependency(op->dstHi, op->lhsLo);
    RecordDependency(op->dstHi, op->lhsHi);
    RecordDependency(op->dstHi, op->rhsLo);
    RecordDependency(op->dstHi, op->rhsHi);
    if (op->setFlags) {
        // TODO: NZ flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRStoreFlagsOp *op) {
    RecordWrite(op->dstCPSR, op);
    RecordDependency(op->dstCPSR, op->srcCPSR);
    RecordDependency(op->dstCPSR, op->values);
    // TODO: NZCVQ flags are written depending on op->flags
}

void DeadStoreEliminationOptimizerPass::Process(IRUpdateFlagsOp *op) {
    RecordWrite(op->dstCPSR, op);
    RecordDependency(op->dstCPSR, op->srcCPSR);
    // TODO: NZCV flags are written depending on op->flags
}

void DeadStoreEliminationOptimizerPass::Process(IRUpdateStickyOverflowOp *op) {
    RecordWrite(op->dstCPSR, op);
    RecordDependency(op->dstCPSR, op->srcCPSR);
    // TODO: Q flag is written depending on op->flags
}

void DeadStoreEliminationOptimizerPass::Process(IRBranchOp *op) {
    RecordRead(op->address);
    // TODO: PC is written, CPSR is read
}

void DeadStoreEliminationOptimizerPass::Process(IRBranchExchangeOp *op) {
    RecordRead(op->address);
    // TODO: PC and CPSR are written, CPSR is read
}

void DeadStoreEliminationOptimizerPass::Process(IRLoadCopRegisterOp *op) {
    RecordWrite(op->dstValue, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRStoreCopRegisterOp *op) {
    RecordRead(op->srcValue);
}

void DeadStoreEliminationOptimizerPass::Process(IRConstantOp *op) {
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRCopyVarOp *op) {
    RecordWrite(op->dst, op);
    RecordRead(op->var);
    RecordDependency(op->dst, op->var);
}

void DeadStoreEliminationOptimizerPass::Process(IRGetBaseVectorAddressOp *op) {
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::RecordWrite(Variable dst, IROp *op) {
    if (!dst.IsPresent()) {
        return;
    }
    auto varIndex = dst.Index();
    ResizeWrites(varIndex);
    m_varWrites[varIndex] = op;
}

void DeadStoreEliminationOptimizerPass::RecordWrite(VariableArg dst, IROp *op) {
    RecordWrite(dst.var, op);
}

void DeadStoreEliminationOptimizerPass::RecordRead(Variable dst) {
    if (!dst.IsPresent()) {
        return;
    }
    auto varIndex = dst.Index();
    if (varIndex >= m_varWrites.size()) {
        return;
    }
    m_varWrites[varIndex] = nullptr;
}

void DeadStoreEliminationOptimizerPass::RecordRead(VariableArg dst) {
    RecordRead(dst.var);
}

void DeadStoreEliminationOptimizerPass::RecordRead(VarOrImmArg dst) {
    if (!dst.immediate) {
        RecordRead(dst.var);
    }
}

void DeadStoreEliminationOptimizerPass::RecordDependency(Variable dst, Variable src) {
    if (!dst.IsPresent() || !src.IsPresent()) {
        return;
    }
    auto varIndex = dst.Index();
    ResizeDependencies(varIndex);
    m_dependencies[varIndex].push_back(src);
    RecordRead(src);
}

void DeadStoreEliminationOptimizerPass::RecordDependency(VariableArg dst, Variable src) {
    RecordDependency(dst.var, src);
}

void DeadStoreEliminationOptimizerPass::RecordDependency(Variable dst, VariableArg src) {
    RecordDependency(dst, src.var);
}

void DeadStoreEliminationOptimizerPass::RecordDependency(VariableArg dst, VariableArg src) {
    RecordDependency(dst, src.var);
}

void DeadStoreEliminationOptimizerPass::RecordDependency(Variable dst, VarOrImmArg src) {
    if (!src.immediate) {
        RecordDependency(dst, src.var);
    }
}

void DeadStoreEliminationOptimizerPass::RecordDependency(VariableArg dst, VarOrImmArg src) {
    if (!src.immediate) {
        RecordDependency(dst, src.var);
    }
}

void DeadStoreEliminationOptimizerPass::ResizeWrites(size_t size) {
    if (m_varWrites.size() <= size) {
        m_varWrites.resize(size + 1);
    }
}

void DeadStoreEliminationOptimizerPass::ResizeDependencies(size_t size) {
    if (m_dependencies.size() <= size) {
        m_dependencies.resize(size + 1);
    }
}

} // namespace armajitto::ir
