#include "dead_var_store_elimination.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

#include <cassert>

namespace armajitto::ir {

DeadVarStoreEliminationOptimizerPass::DeadVarStoreEliminationOptimizerPass(Emitter &emitter,
                                                                           std::pmr::monotonic_buffer_resource &buffer)
    : DeadStoreEliminationOptimizerPassBase(emitter)
    , m_buffer(buffer)
    , m_varWrites(&buffer)
    , m_dependencies(&buffer) {

    const uint32_t varCount = emitter.VariableCount();
    m_varWrites.resize(varCount);
    m_dependencies.resize(varCount, std::pmr::vector<Variable>{&buffer});
}

void DeadVarStoreEliminationOptimizerPass::PostProcessImpl() {
    // Reset all unread variables
    for (size_t i = 0; i < m_varWrites.size(); i++) {
        auto &write = m_varWrites[i];
        if (write.op != nullptr && !write.read) {
            ResetVariableRecursive(Variable{i}, write.op);
        }
    }
}

void DeadVarStoreEliminationOptimizerPass::Process(IRGetRegisterOp *op) {
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRSetRegisterOp *op) {
    RecordRead(op->src);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRGetCPSROp *op) {
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRSetCPSROp *op) {
    RecordRead(op->src);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRGetSPSROp *op) {
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRSetSPSROp *op) {
    RecordRead(op->src);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRMemReadOp *op) {
    RecordRead(op->address, true);
    RecordDependentRead(op->dst, op->address);
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRMemWriteOp *op) {
    RecordRead(op->src);
    RecordRead(op->address);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRPreloadOp *op) {
    RecordRead(op->address);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRLogicalShiftLeftOp *op) {
    RecordRead(op->value, true);
    RecordRead(op->amount, true);
    RecordDependentRead(op->dst, op->value);
    RecordDependentRead(op->dst, op->amount);
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    RecordRead(op->value, true);
    RecordRead(op->amount, true);
    RecordDependentRead(op->dst, op->value);
    RecordDependentRead(op->dst, op->amount);
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    RecordRead(op->value, true);
    RecordRead(op->amount, true);
    RecordDependentRead(op->dst, op->value);
    RecordDependentRead(op->dst, op->amount);
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRRotateRightOp *op) {
    RecordRead(op->value, true);
    RecordRead(op->amount, true);
    RecordDependentRead(op->dst, op->value);
    RecordDependentRead(op->dst, op->amount);
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRRotateRightExtendedOp *op) {
    RecordRead(op->value, true);
    RecordDependentRead(op->dst, op->value);
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRBitwiseAndOp *op) {
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRBitwiseOrOp *op) {
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRBitwiseXorOp *op) {
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRBitClearOp *op) {
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRCountLeadingZerosOp *op) {
    RecordRead(op->value, true);
    RecordDependentRead(op->dst, op->value);
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRAddOp *op) {
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRAddCarryOp *op) {
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRSubtractOp *op) {
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRSubtractCarryOp *op) {
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRMoveOp *op) {
    RecordRead(op->value, op->flags != arm::Flags::None);
    RecordDependentRead(op->dst, op->value);
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRMoveNegatedOp *op) {
    RecordRead(op->value, true);
    RecordDependentRead(op->dst, op->value);
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRSaturatingAddOp *op) {
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRMultiplyOp *op) {
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRMultiplyLongOp *op) {
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dstLo, op->lhs);
    RecordDependentRead(op->dstLo, op->rhs);
    RecordDependentRead(op->dstHi, op->lhs);
    RecordDependentRead(op->dstHi, op->rhs);
    RecordWrite(op->dstLo, op);
    RecordWrite(op->dstHi, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRAddLongOp *op) {
    RecordRead(op->lhsLo, true);
    RecordRead(op->lhsHi, true);
    RecordRead(op->rhsLo, true);
    RecordRead(op->rhsHi, true);
    RecordDependentRead(op->dstLo, op->lhsLo);
    RecordDependentRead(op->dstLo, op->lhsHi);
    RecordDependentRead(op->dstLo, op->rhsLo);
    RecordDependentRead(op->dstLo, op->rhsHi);
    RecordDependentRead(op->dstHi, op->lhsLo);
    RecordDependentRead(op->dstHi, op->lhsHi);
    RecordDependentRead(op->dstHi, op->rhsLo);
    RecordDependentRead(op->dstHi, op->rhsHi);
    RecordWrite(op->dstLo, op);
    RecordWrite(op->dstHi, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRLoadFlagsOp *op) {
    RecordRead(op->srcCPSR, true);
    RecordDependentRead(op->dstCPSR, op->srcCPSR);
    RecordWrite(op->dstCPSR, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRLoadStickyOverflowOp *op) {
    RecordRead(op->srcCPSR, true);
    RecordDependentRead(op->dstCPSR, op->srcCPSR);
    RecordWrite(op->dstCPSR, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRBranchOp *op) {
    RecordRead(op->address);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRBranchExchangeOp *op) {
    RecordRead(op->address);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRLoadCopRegisterOp *op) {
    RecordWrite(op->dstValue, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRStoreCopRegisterOp *op) {
    RecordRead(op->srcValue);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRConstantOp *op) {
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRCopyVarOp *op) {
    RecordRead(op->var, false);
    RecordDependentRead(op->dst, op->var);
    RecordWrite(op->dst, op);
}

void DeadVarStoreEliminationOptimizerPass::Process(IRGetBaseVectorAddressOp *op) {
    RecordWrite(op->dst, op);
}

// ---------------------------------------------------------------------------------------------------------------------
// Variable read, write and consumption tracking

void DeadVarStoreEliminationOptimizerPass::RecordRead(VariableArg &dst, bool consume) {
    if (!dst.var.IsPresent()) {
        return;
    }
    auto varIndex = dst.var.Index();
    if (varIndex >= m_varWrites.size()) {
        return;
    }
    m_varWrites[varIndex].read = true;
    if (consume) {
        m_varWrites[varIndex].consumed = true;
    }
}

void DeadVarStoreEliminationOptimizerPass::RecordRead(VarOrImmArg &dst, bool consume) {
    if (!dst.immediate) {
        RecordRead(dst.var, consume);
    }
}

void DeadVarStoreEliminationOptimizerPass::RecordDependentRead(VariableArg dst, Variable src) {
    if (!dst.var.IsPresent() || !src.IsPresent()) {
        return;
    }
    auto varIndex = dst.var.Index();
    ResizeDependencies(varIndex);
    m_dependencies[varIndex].push_back(src);
}

void DeadVarStoreEliminationOptimizerPass::RecordDependentRead(VariableArg dst, VariableArg src) {
    RecordDependentRead(dst, src.var);
}

void DeadVarStoreEliminationOptimizerPass::RecordDependentRead(VariableArg dst, VarOrImmArg src) {
    if (!src.immediate) {
        RecordDependentRead(dst, src.var);
    }
}

void DeadVarStoreEliminationOptimizerPass::RecordWrite(VariableArg dst, IROp *op) {
    if (!dst.var.IsPresent()) {
        return;
    }
    auto varIndex = dst.var.Index();
    ResizeWrites(varIndex);
    m_varWrites[varIndex].op = op;
    m_varWrites[varIndex].read = false;
    m_varWrites[varIndex].consumed = false;
}

void DeadVarStoreEliminationOptimizerPass::ResizeWrites(size_t index) {
    if (m_varWrites.size() <= index) {
        m_varWrites.resize(index + 1);
    }
}

void DeadVarStoreEliminationOptimizerPass::ResizeDependencies(size_t index) {
    if (m_dependencies.size() <= index) {
        m_dependencies.resize(index + 1, std::pmr::vector<Variable>{&m_buffer});
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Generic ResetVariable for variables

void DeadVarStoreEliminationOptimizerPass::ResetVariableRecursive(Variable var, IROp *op) {
    if (!var.IsPresent()) {
        return;
    }

    bool erased = VisitIROp(op, [this, var](auto op) {
        ResetVariable(var, op);
        return IsDeadInstruction(op);
    });

    // Follow dependencies
    if (erased && var.Index() < m_dependencies.size()) {
        for (auto &dep : m_dependencies[var.Index()]) {
            if (dep.IsPresent()) {
                auto &write = m_varWrites[dep.Index()];
                if (write.op != nullptr && !write.consumed) {
                    ResetVariableRecursive(dep, write.op);
                }
            }
        }
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRGetRegisterOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRGetCPSROp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRGetSPSROp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRMemReadOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRLogicalShiftLeftOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRLogicalShiftRightOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRArithmeticShiftRightOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRRotateRightOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRRotateRightExtendedOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRBitwiseAndOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRBitwiseOrOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRBitwiseXorOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRBitClearOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRCountLeadingZerosOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRAddOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRAddCarryOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRSubtractOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRSubtractCarryOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRMoveOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRMoveNegatedOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRSaturatingAddOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRSaturatingSubtractOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRMultiplyOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRMultiplyLongOp *op) {
    if (op->dstLo == var) {
        MarkDirty();
        op->dstLo.var = {};
    }
    if (op->dstHi == var) {
        MarkDirty();
        op->dstHi.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRAddLongOp *op) {
    if (op->dstLo == var) {
        MarkDirty();
        op->dstLo.var = {};
    }
    if (op->dstHi == var) {
        MarkDirty();
        op->dstHi.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRLoadFlagsOp *op) {
    if (op->dstCPSR == var) {
        MarkDirty();
        op->dstCPSR.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRLoadStickyOverflowOp *op) {
    if (op->dstCPSR == var) {
        MarkDirty();
        op->dstCPSR.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRLoadCopRegisterOp *op) {
    if (op->dstValue == var) {
        MarkDirty();
        op->dstValue.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRConstantOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRCopyVarOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

void DeadVarStoreEliminationOptimizerPass::ResetVariable(Variable var, IRGetBaseVectorAddressOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
}

} // namespace armajitto::ir
