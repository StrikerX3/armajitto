#include "dead_store_elimination.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

namespace armajitto::ir {

void DeadStoreEliminationOptimizerPass::PostProcess() {
    // Erase all unread writes to variables
    for (size_t i = 0; i < m_varWrites.size(); i++) {
        auto &write = m_varWrites[i];
        if (write.op != nullptr && !write.read) {
            EraseWriteRecursive(Variable{i}, write.op);
        }
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRGetRegisterOp *op) {
    RecordRead(op->src);
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRSetRegisterOp *op) {
    RecordRead(op->src);
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRGetCPSROp *op) {
    // TODO: read from CPSR
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRSetCPSROp *op) {
    RecordRead(op->src);
    // TODO: write to CPSR
}

void DeadStoreEliminationOptimizerPass::Process(IRGetSPSROp *op) {
    // TODO: read from SPSR
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRSetSPSROp *op) {
    RecordRead(op->src);
    // TODO: write to SPSR
}

void DeadStoreEliminationOptimizerPass::Process(IRMemReadOp *op) {
    RecordDependentRead(op->dst, op->address);
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRMemWriteOp *op) {
    RecordRead(op->src);
    RecordRead(op->address);
}

void DeadStoreEliminationOptimizerPass::Process(IRPreloadOp *op) {
    RecordRead(op->address);
}

void DeadStoreEliminationOptimizerPass::Process(IRLogicalShiftLeftOp *op) {
    RecordDependentRead(op->dst, op->value);
    RecordDependentRead(op->dst, op->amount);
    if (op->setCarry) {
        RecordWrite(arm::Flags::C, op);
    }
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    RecordDependentRead(op->dst, op->value);
    RecordDependentRead(op->dst, op->amount);
    if (op->setCarry) {
        RecordWrite(arm::Flags::C, op);
    }
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    RecordDependentRead(op->dst, op->value);
    RecordDependentRead(op->dst, op->amount);
    if (op->setCarry) {
        RecordWrite(arm::Flags::C, op);
    }
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRRotateRightOp *op) {
    RecordDependentRead(op->dst, op->value);
    RecordDependentRead(op->dst, op->amount);
    if (op->setCarry) {
        RecordWrite(arm::Flags::C, op);
    }
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRRotateRightExtendOp *op) {
    RecordRead(arm::Flags::C);
    RecordDependentRead(op->dst, op->value);
    RecordWrite(op->dst, op);
    if (op->setCarry) {
        RecordWrite(arm::Flags::C, op);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRBitwiseAndOp *op) {
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordWrite(op->flags, op);
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRBitwiseOrOp *op) {
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordWrite(op->flags, op);
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRBitwiseXorOp *op) {
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordWrite(op->flags, op);
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRBitClearOp *op) {
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordWrite(op->flags, op);
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRCountLeadingZerosOp *op) {
    RecordDependentRead(op->dst, op->value);
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRAddOp *op) {
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordWrite(op->flags, op);
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRAddCarryOp *op) {
    RecordRead(arm::Flags::C);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordWrite(op->flags, op);
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRSubtractOp *op) {
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordWrite(op->flags, op);
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRSubtractCarryOp *op) {
    RecordRead(arm::Flags::C);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordWrite(op->flags, op);
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRMoveOp *op) {
    if (op->flags != arm::Flags::None) {
        RecordDependentRead(op->dst, op->value);
        RecordWrite(op->flags, op);
    } else {
        RecordDependentRead(op->dst, op->value, false);
    }
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRMoveNegatedOp *op) {
    RecordDependentRead(op->dst, op->value);
    RecordWrite(op->flags, op);
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRSaturatingAddOp *op) {
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordWrite(op->flags, op);
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordWrite(op->flags, op);
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRMultiplyOp *op) {
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordWrite(op->flags, op);
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRMultiplyLongOp *op) {
    RecordDependentRead(op->dstLo, op->lhs);
    RecordDependentRead(op->dstLo, op->rhs);
    RecordDependentRead(op->dstHi, op->lhs);
    RecordDependentRead(op->dstHi, op->rhs);
    RecordWrite(op->flags, op);
    RecordWrite(op->dstLo, op);
    RecordWrite(op->dstHi, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRAddLongOp *op) {
    RecordDependentRead(op->dstLo, op->lhsLo);
    RecordDependentRead(op->dstLo, op->lhsHi);
    RecordDependentRead(op->dstLo, op->rhsLo);
    RecordDependentRead(op->dstLo, op->rhsHi);
    RecordDependentRead(op->dstHi, op->lhsLo);
    RecordDependentRead(op->dstHi, op->lhsHi);
    RecordDependentRead(op->dstHi, op->rhsLo);
    RecordDependentRead(op->dstHi, op->rhsHi);
    RecordWrite(op->flags, op);
    RecordWrite(op->dstLo, op);
    RecordWrite(op->dstHi, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRStoreFlagsOp *op) {
    RecordDependentRead(op->dstCPSR, op->srcCPSR);
    RecordDependentRead(op->dstCPSR, op->values);
    RecordWrite(op->flags, op);
    RecordWrite(op->dstCPSR, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRUpdateFlagsOp *op) {
    RecordRead(op->flags);
    RecordDependentRead(op->dstCPSR, op->srcCPSR);
    RecordWrite(op->dstCPSR, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRUpdateStickyOverflowOp *op) {
    RecordRead(arm::Flags::Q);
    RecordDependentRead(op->dstCPSR, op->srcCPSR);
    RecordWrite(op->dstCPSR, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRBranchOp *op) {
    RecordRead(op->address);
    // TODO: read from CPSR
    RecordWrite(arm::GPR::PC, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRBranchExchangeOp *op) {
    RecordRead(op->address);
    // TODO: read from CPSR
    RecordWrite(arm::GPR::PC, op);
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
    RecordDependentRead(op->dst, op->var, false);
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRGetBaseVectorAddressOp *op) {
    RecordWrite(op->dst, op);
}

// ---------------------------------------------------------------------------------------------------------------------

void DeadStoreEliminationOptimizerPass::RecordRead(Variable dst, bool consume) {
    if (!dst.IsPresent()) {
        return;
    }
    auto varIndex = dst.Index();
    if (varIndex >= m_varWrites.size()) {
        return;
    }
    m_varWrites[varIndex].read = true;
    if (consume) {
        m_varWrites[varIndex].consumed = true;
    }
}

void DeadStoreEliminationOptimizerPass::RecordRead(VariableArg dst, bool consume) {
    RecordRead(dst.var, consume);
}

void DeadStoreEliminationOptimizerPass::RecordRead(VarOrImmArg dst, bool consume) {
    if (!dst.immediate) {
        RecordRead(dst.var, consume);
    }
}

void DeadStoreEliminationOptimizerPass::RecordDependentRead(Variable dst, Variable src, bool consume) {
    if (!dst.IsPresent() || !src.IsPresent()) {
        return;
    }
    auto varIndex = dst.Index();
    ResizeDependencies(varIndex);
    m_dependencies[varIndex].push_back(src);
    RecordRead(src, consume);
}

void DeadStoreEliminationOptimizerPass::RecordDependentRead(VariableArg dst, Variable src, bool consume) {
    RecordDependentRead(dst.var, src, consume);
}

void DeadStoreEliminationOptimizerPass::RecordDependentRead(Variable dst, VariableArg src, bool consume) {
    RecordDependentRead(dst, src.var, consume);
}

void DeadStoreEliminationOptimizerPass::RecordDependentRead(VariableArg dst, VariableArg src, bool consume) {
    RecordDependentRead(dst, src.var, consume);
}

void DeadStoreEliminationOptimizerPass::RecordDependentRead(Variable dst, VarOrImmArg src, bool consume) {
    if (!src.immediate) {
        RecordDependentRead(dst, src.var, consume);
    }
}

void DeadStoreEliminationOptimizerPass::RecordDependentRead(VariableArg dst, VarOrImmArg src, bool consume) {
    if (!src.immediate) {
        RecordDependentRead(dst, src.var, consume);
    }
}

void DeadStoreEliminationOptimizerPass::RecordWrite(Variable dst, IROp *op) {
    if (!dst.IsPresent()) {
        return;
    }
    auto varIndex = dst.Index();
    ResizeWrites(varIndex);
    m_varWrites[varIndex].op = op;
    m_varWrites[varIndex].read = false;
    m_varWrites[varIndex].consumed = false;
}

void DeadStoreEliminationOptimizerPass::RecordWrite(VariableArg dst, IROp *op) {
    RecordWrite(dst.var, op);
}

void DeadStoreEliminationOptimizerPass::RecordRead(GPRArg gpr) {
    auto gprIndex = MakeGPRIndex(gpr);
    m_gprWrites[gprIndex] = nullptr; // Leave instruction alone
}

void DeadStoreEliminationOptimizerPass::RecordWrite(GPRArg gpr, IROp *op) {
    auto gprIndex = MakeGPRIndex(gpr);
    IROp *writeOp = m_gprWrites[gprIndex];
    if (writeOp != nullptr) {
        // GPR is overwritten; erase previous instruction, which is always going to be an IRSetRegisterOp
        m_emitter.Erase(writeOp);
    }
    m_gprWrites[gprIndex] = op;
}

void DeadStoreEliminationOptimizerPass::RecordRead(arm::Flags flags) {
    // Leave corresponding instructions alone
    auto bmFlags = BitmaskEnum(flags);
    auto clearFlag = [&](arm::Flags flag, IROp *&write) {
        if (bmFlags.AnyOf(flag)) {
            if (write != nullptr) {
                m_flagsRead[write] |= flag;
            }
            write = nullptr;
        }
    };
    clearFlag(arm::Flags::N, m_nWrite);
    clearFlag(arm::Flags::Z, m_zWrite);
    clearFlag(arm::Flags::C, m_cWrite);
    clearFlag(arm::Flags::V, m_vWrite);
    clearFlag(arm::Flags::Q, m_qWrite);
}

void DeadStoreEliminationOptimizerPass::RecordWrite(arm::Flags flags, IROp *op) {
    auto bmFlags = BitmaskEnum(flags);
    if (bmFlags.None()) {
        return;
    }
    auto record = [&](arm::Flags flag, IROp *&write) {
        if (bmFlags.AnyOf(flag)) {
            if (write != nullptr) {
                VisitIROp(write, [this, flag](auto op) -> void { EraseWrite(flag, op); });
            }
            m_flagsRead[op] |= flag;
            write = op;
        }
    };
    record(arm::Flags::N, m_nWrite);
    record(arm::Flags::Z, m_zWrite);
    record(arm::Flags::C, m_cWrite);
    record(arm::Flags::V, m_vWrite);
    record(arm::Flags::Q, m_qWrite);
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

// ---------------------------------------------------------------------------------------------------------------------

void DeadStoreEliminationOptimizerPass::EraseWriteRecursive(Variable var, IROp *op) {
    if (!var.IsPresent()) {
        return;
    }

    bool erased = VisitIROp(op, [this, var](auto op) { return EraseWrite(var, op); });

    // Follow dependencies
    if (erased) {
        if (var.Index() < m_dependencies.size()) {
            for (auto &dep : m_dependencies[var.Index()]) {
                if (dep.IsPresent()) {
                    auto &write = m_varWrites[dep.Index()];
                    if (!write.consumed) {
                        EraseWriteRecursive(dep, write.op);
                    }
                }
            }
        }
    }
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRGetRegisterOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRGetCPSROp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRGetSPSROp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRMemReadOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRLogicalShiftLeftOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRLogicalShiftRightOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRArithmeticShiftRightOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRRotateRightOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRRotateRightExtendOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRBitwiseAndOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRBitwiseOrOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRBitwiseXorOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRBitClearOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRCountLeadingZerosOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRAddOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRAddCarryOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRSubtractOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRSubtractCarryOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRMoveOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRMoveNegatedOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRSaturatingAddOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRSaturatingSubtractOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRMultiplyOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRMultiplyLongOp *op) {
    if (op->dstLo == var) {
        op->dstLo.var = {};
    }
    if (op->dstHi == var) {
        op->dstHi.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRAddLongOp *op) {
    if (op->dstLo == var) {
        op->dstLo.var = {};
    }
    if (op->dstHi == var) {
        op->dstHi.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRStoreFlagsOp *op) {
    if (op->dstCPSR == var) {
        op->dstCPSR.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRUpdateFlagsOp *op) {
    if (op->dstCPSR == var) {
        op->dstCPSR.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRUpdateStickyOverflowOp *op) {
    if (op->dstCPSR == var) {
        op->dstCPSR.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRLoadCopRegisterOp *op) {
    if (op->dstValue == var) {
        op->dstValue.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRConstantOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRCopyVarOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRGetBaseVectorAddressOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    return EraseInstruction(op);
}

// ---------------------------------------------------------------------------------------------------------------------

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRLogicalShiftLeftOp *op) {
    if (BitmaskEnum(flag).AnyOf(arm::Flags::C)) {
        op->setCarry = false;
    }
}

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRLogicalShiftRightOp *op) {
    if (BitmaskEnum(flag).AnyOf(arm::Flags::C)) {
        op->setCarry = false;
    }
}

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRArithmeticShiftRightOp *op) {
    if (BitmaskEnum(flag).AnyOf(arm::Flags::C)) {
        op->setCarry = false;
    }
}

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRRotateRightOp *op) {
    if (BitmaskEnum(flag).AnyOf(arm::Flags::C)) {
        op->setCarry = false;
    }
}

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRRotateRightExtendOp *op) {
    if (BitmaskEnum(flag).AnyOf(arm::Flags::C)) {
        op->setCarry = false;
    }
}

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRBitwiseAndOp *op) {
    op->flags &= ~flag;
}

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRBitwiseOrOp *op) {
    op->flags &= ~flag;
}

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRBitwiseXorOp *op) {
    op->flags &= ~flag;
}

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRBitClearOp *op) {
    op->flags &= ~flag;
}

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRAddOp *op) {
    op->flags &= ~flag;
}

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRAddCarryOp *op) {
    op->flags &= ~flag;
}

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRSubtractOp *op) {
    op->flags &= ~flag;
}

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRSubtractCarryOp *op) {
    op->flags &= ~flag;
}

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRMoveOp *op) {
    op->flags &= ~flag;
}

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRMoveNegatedOp *op) {
    op->flags &= ~flag;
}

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRSaturatingAddOp *op) {
    op->flags &= ~flag;
}

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRSaturatingSubtractOp *op) {
    op->flags &= ~flag;
}

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRMultiplyOp *op) {
    op->flags &= ~flag;
}

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRMultiplyLongOp *op) {
    op->flags &= ~flag;
}

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRAddLongOp *op) {
    op->flags &= ~flag;
}

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRStoreFlagsOp *op) {
    op->flags &= ~flag;
    if (op->flags == arm::Flags::None) {
        m_emitter.Overwrite(op).CopyVar(op->dstCPSR, op->srcCPSR);
    } else if (op->values.immediate) {
        op->values.imm.value &= static_cast<uint32_t>(~flag);
    }
}

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRUpdateFlagsOp *op) {
    op->flags &= ~flag;
    if (op->flags == arm::Flags::None) {
        m_emitter.Overwrite(op).CopyVar(op->dstCPSR, op->srcCPSR);
    }
}

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRUpdateStickyOverflowOp *op) {
    if (flag == arm::Flags::Q) {
        m_emitter.Overwrite(op).CopyVar(op->dstCPSR, op->srcCPSR);
    }
}

// ---------------------------------------------------------------------------------------------------------------------

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRGetRegisterOp *op) {
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRSetRegisterOp *op) {
    // TODO: implement
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRGetCPSROp *op) {
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRSetCPSROp *op) {
    // TODO: implement
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRGetSPSROp *op) {
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRSetSPSROp *op) {
    // TODO: implement
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRMemReadOp *op) {
    if (!op->dst.var.IsPresent()) {
        if (op->address.immediate && false /* TODO: no side effects on address */) {
            m_emitter.Erase(op);
            return true;
        }
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRLogicalShiftLeftOp *op) {
    if (!op->dst.var.IsPresent() && (op->setCarry || BitmaskEnum(m_flagsRead[op]).NoneOf(arm::Flags::C))) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRLogicalShiftRightOp *op) {
    if (!op->dst.var.IsPresent() && (!op->setCarry || BitmaskEnum(m_flagsRead[op]).NoneOf(arm::Flags::C))) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRArithmeticShiftRightOp *op) {
    if (!op->dst.var.IsPresent() && (!op->setCarry || BitmaskEnum(m_flagsRead[op]).NoneOf(arm::Flags::C))) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRRotateRightOp *op) {
    if (!op->dst.var.IsPresent() && (!op->setCarry || BitmaskEnum(m_flagsRead[op]).NoneOf(arm::Flags::C))) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRRotateRightExtendOp *op) {
    if (!op->dst.var.IsPresent() && (!op->setCarry || BitmaskEnum(m_flagsRead[op]).NoneOf(arm::Flags::C))) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRBitwiseAndOp *op) {
    if (!op->dst.var.IsPresent() && BitmaskEnum(m_flagsRead[op]).NoneOf(op->flags)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRBitwiseOrOp *op) {
    if (!op->dst.var.IsPresent() && BitmaskEnum(m_flagsRead[op]).NoneOf(op->flags)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRBitwiseXorOp *op) {
    if (!op->dst.var.IsPresent() && BitmaskEnum(m_flagsRead[op]).NoneOf(op->flags)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRBitClearOp *op) {
    if (!op->dst.var.IsPresent() && BitmaskEnum(m_flagsRead[op]).NoneOf(op->flags)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRCountLeadingZerosOp *op) {
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRAddOp *op) {
    if (!op->dst.var.IsPresent() && BitmaskEnum(m_flagsRead[op]).NoneOf(op->flags)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRAddCarryOp *op) {
    if (!op->dst.var.IsPresent() && BitmaskEnum(m_flagsRead[op]).NoneOf(op->flags)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRSubtractOp *op) {
    if (!op->dst.var.IsPresent() && BitmaskEnum(m_flagsRead[op]).NoneOf(op->flags)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRSubtractCarryOp *op) {
    if (!op->dst.var.IsPresent() && BitmaskEnum(m_flagsRead[op]).NoneOf(op->flags)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRMoveOp *op) {
    if (!op->dst.var.IsPresent() && BitmaskEnum(m_flagsRead[op]).NoneOf(op->flags)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRMoveNegatedOp *op) {
    if (!op->dst.var.IsPresent() && BitmaskEnum(m_flagsRead[op]).NoneOf(op->flags)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRSaturatingAddOp *op) {
    if (!op->dst.var.IsPresent() && BitmaskEnum(m_flagsRead[op]).NoneOf(op->flags)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRSaturatingSubtractOp *op) {
    if (!op->dst.var.IsPresent() && BitmaskEnum(m_flagsRead[op]).NoneOf(op->flags)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRMultiplyOp *op) {
    if (!op->dst.var.IsPresent() && BitmaskEnum(m_flagsRead[op]).NoneOf(op->flags)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRMultiplyLongOp *op) {
    if (!op->dstLo.var.IsPresent() && !op->dstHi.var.IsPresent() && BitmaskEnum(m_flagsRead[op]).NoneOf(op->flags)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRAddLongOp *op) {
    if (!op->dstLo.var.IsPresent() && !op->dstHi.var.IsPresent() && BitmaskEnum(m_flagsRead[op]).NoneOf(op->flags)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRStoreFlagsOp *op) {
    if (!op->dstCPSR.var.IsPresent() && BitmaskEnum(m_flagsRead[op]).None()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRUpdateFlagsOp *op) {
    if (!op->dstCPSR.var.IsPresent() && BitmaskEnum(m_flagsRead[op]).None()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRUpdateStickyOverflowOp *op) {
    if (!op->dstCPSR.var.IsPresent() && BitmaskEnum(m_flagsRead[op]).NoneOf(arm::Flags::Q)) {
        m_emitter.Erase(op);
        return true;
    } else if (BitmaskEnum(m_flagsRead[op]).None()) {
        m_emitter.Overwrite().CopyVar(op->dstCPSR, op->srcCPSR);
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRLoadCopRegisterOp *op) {
    if (!op->dstValue.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRConstantOp *op) {
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRCopyVarOp *op) {
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRGetBaseVectorAddressOp *op) {
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

} // namespace armajitto::ir
