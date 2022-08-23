#include "dead_store_elimination.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

#include <cassert>

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
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordGPRRead(op->src);
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRSetRegisterOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordRead(op->src);
    RecordGPRWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRGetCPSROp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    if (RecordAndEraseDeadCPSRRead(op->dst, op)) {
        return;
    }
    RecordWrite(op->dst, op);
    InitFlagWrites(op->dst);
}

void DeadStoreEliminationOptimizerPass::Process(IRSetCPSROp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordRead(op->src);
    if (!op->src.immediate) {
        RecordCPSRWrite(op->src.var, op);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRGetSPSROp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordSPSRRead(op->mode);
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRSetSPSROp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordRead(op->src);
    RecordSPSRWrite(op->mode, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRMemReadOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordRead(op->address, true);
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
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordRead(op->value, true);
    RecordRead(op->amount, true);
    RecordDependentRead(op->dst, op->value);
    RecordDependentRead(op->dst, op->amount);
    if (op->setCarry) {
        RecordHostFlagsWrite(arm::Flags::C, op);
    }
    RecordWrite(op->dst, op);
    if (HasCPSRVersion(op->value)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordRead(op->value, true);
    RecordRead(op->amount, true);
    RecordDependentRead(op->dst, op->value);
    RecordDependentRead(op->dst, op->amount);
    if (op->setCarry) {
        RecordHostFlagsWrite(arm::Flags::C, op);
    }
    RecordWrite(op->dst, op);
    if (HasCPSRVersion(op->value)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordRead(op->value, true);
    RecordRead(op->amount, true);
    RecordDependentRead(op->dst, op->value);
    RecordDependentRead(op->dst, op->amount);
    if (op->setCarry) {
        RecordHostFlagsWrite(arm::Flags::C, op);
    }
    RecordWrite(op->dst, op);
    if (HasCPSRVersion(op->value)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRRotateRightOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordRead(op->value, true);
    RecordRead(op->amount, true);
    RecordDependentRead(op->dst, op->value);
    RecordDependentRead(op->dst, op->amount);
    if (op->setCarry) {
        RecordHostFlagsWrite(arm::Flags::C, op);
    }
    RecordWrite(op->dst, op);
    if (HasCPSRVersion(op->value)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRRotateRightExtendedOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordHostFlagsRead(arm::Flags::C);
    RecordRead(op->value, true);
    RecordDependentRead(op->dst, op->value);
    RecordWrite(op->dst, op);
    if (op->setCarry) {
        RecordHostFlagsWrite(arm::Flags::C, op);
    }
    if (HasCPSRVersion(op->value)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRBitwiseAndOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);
    if (HasCPSRVersion(op->lhs) || HasCPSRVersion(op->rhs)) {
        AssignNewCPSRVersion(op->dst);
    }
    if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
        auto [imm, var] = *split;
        RecordFlagWrites(op->dst, var, static_cast<arm::Flags>(imm), op);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRBitwiseOrOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);
    if (HasCPSRVersion(op->lhs) || HasCPSRVersion(op->rhs)) {
        AssignNewCPSRVersion(op->dst);
    }
    if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
        auto [imm, var] = *split;
        RecordFlagWrites(op->dst, var, static_cast<arm::Flags>(imm), op);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRBitwiseXorOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);
    if (HasCPSRVersion(op->lhs) || HasCPSRVersion(op->rhs)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRBitClearOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);
    if (HasCPSRVersion(op->lhs) || HasCPSRVersion(op->rhs)) {
        AssignNewCPSRVersion(op->dst);
    }
    if (auto split = SplitImmVarPair(op->lhs, op->rhs)) {
        auto [imm, var] = *split;
        RecordFlagWrites(op->dst, var, static_cast<arm::Flags>(imm), op);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRCountLeadingZerosOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordRead(op->value, true);
    RecordDependentRead(op->dst, op->value);
    RecordWrite(op->dst, op);
    if (HasCPSRVersion(op->value)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRAddOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);
    if (HasCPSRVersion(op->lhs) || HasCPSRVersion(op->rhs)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRAddCarryOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordHostFlagsRead(arm::Flags::C);
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);
    if (HasCPSRVersion(op->lhs) || HasCPSRVersion(op->rhs)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRSubtractOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);
    if (HasCPSRVersion(op->lhs) || HasCPSRVersion(op->rhs)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRSubtractCarryOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordHostFlagsRead(arm::Flags::C);
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);
    if (HasCPSRVersion(op->lhs) || HasCPSRVersion(op->rhs)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRMoveOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordRead(op->value, op->flags != arm::Flags::None);
    RecordDependentRead(op->dst, op->value);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);
    if (!op->value.immediate) {
        CopyCPSRVersion(op->dst, op->value.var);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRMoveNegatedOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordRead(op->value, true);
    RecordDependentRead(op->dst, op->value);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);
    if (HasCPSRVersion(op->value)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRSaturatingAddOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);
    if (HasCPSRVersion(op->lhs) || HasCPSRVersion(op->rhs)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);
    if (HasCPSRVersion(op->lhs) || HasCPSRVersion(op->rhs)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRMultiplyOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);
    if (HasCPSRVersion(op->lhs) || HasCPSRVersion(op->rhs)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRMultiplyLongOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dstLo, op->lhs);
    RecordDependentRead(op->dstLo, op->rhs);
    RecordDependentRead(op->dstHi, op->lhs);
    RecordDependentRead(op->dstHi, op->rhs);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dstLo, op);
    RecordWrite(op->dstHi, op);
    if (HasCPSRVersion(op->lhs) || HasCPSRVersion(op->rhs)) {
        AssignNewCPSRVersion(op->dstLo);
        AssignNewCPSRVersion(op->dstHi);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRAddLongOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
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
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dstLo, op);
    RecordWrite(op->dstHi, op);
    if (HasCPSRVersion(op->lhsLo) || HasCPSRVersion(op->lhsHi) || HasCPSRVersion(op->rhsLo) ||
        HasCPSRVersion(op->rhsHi)) {
        AssignNewCPSRVersion(op->dstLo);
        AssignNewCPSRVersion(op->dstHi);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRStoreFlagsOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordHostFlagsWrite(op->flags, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRLoadFlagsOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordHostFlagsRead(op->flags);
    RecordRead(op->srcCPSR, true);
    RecordDependentRead(op->dstCPSR, op->srcCPSR);
    RecordWrite(op->dstCPSR, op);
    if (HasCPSRVersion(op->srcCPSR)) {
        AssignNewCPSRVersion(op->dstCPSR);
    }
    if (!op->srcCPSR.immediate) {
        RecordFlagWrites(op->dstCPSR, op->srcCPSR.var, op->flags, op);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRLoadStickyOverflowOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    if (op->setQ) {
        RecordHostFlagsRead(arm::Flags::Q);
    }
    RecordRead(op->srcCPSR, true);
    RecordDependentRead(op->dstCPSR, op->srcCPSR);
    RecordWrite(op->dstCPSR, op);
    if (HasCPSRVersion(op->srcCPSR)) {
        AssignNewCPSRVersion(op->dstCPSR);
    }
    if (!op->srcCPSR.immediate && op->setQ) {
        RecordFlagWrites(op->dstCPSR, op->srcCPSR.var, arm::Flags::Q, op);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRBranchOp *op) {
    RecordRead(op->address);
    RecordGPRWrite(arm::GPR::PC, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRBranchExchangeOp *op) {
    RecordRead(op->address);
    RecordGPRWrite(arm::GPR::PC, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRLoadCopRegisterOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordWrite(op->dstValue, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRStoreCopRegisterOp *op) {
    RecordRead(op->srcValue);
}

void DeadStoreEliminationOptimizerPass::Process(IRConstantOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRCopyVarOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordRead(op->var, false);
    RecordDependentRead(op->dst, op->var);
    RecordWrite(op->dst, op);
    CopyCPSRVersion(op->dst, op->var);
}

void DeadStoreEliminationOptimizerPass::Process(IRGetBaseVectorAddressOp *op) {
    if (EraseDeadInstruction(op)) {
        return;
    }
    RecordWrite(op->dst, op);
}

// ---------------------------------------------------------------------------------------------------------------------
// Variable read, write and consumption tracking

void DeadStoreEliminationOptimizerPass::RecordRead(VariableArg &dst, bool consume) {
    if (!dst.var.IsPresent()) {
        return;
    }
    SubstituteCPSRVar(dst);
    auto varIndex = dst.var.Index();
    if (varIndex >= m_varWrites.size()) {
        return;
    }
    m_varWrites[varIndex].read = true;
    if (consume) {
        m_varWrites[varIndex].consumed = true;
    }
}

void DeadStoreEliminationOptimizerPass::RecordRead(VarOrImmArg &dst, bool consume) {
    if (!dst.immediate) {
        RecordRead(dst.var, consume);
    }
}

void DeadStoreEliminationOptimizerPass::RecordDependentRead(VariableArg dst, Variable src) {
    if (!dst.var.IsPresent() || !src.IsPresent()) {
        return;
    }
    auto varIndex = dst.var.Index();
    ResizeDependencies(varIndex);
    m_dependencies[varIndex].push_back(src);
}

void DeadStoreEliminationOptimizerPass::RecordDependentRead(VariableArg dst, VariableArg src) {
    RecordDependentRead(dst, src.var);
}

void DeadStoreEliminationOptimizerPass::RecordDependentRead(VariableArg dst, VarOrImmArg src) {
    if (!src.immediate) {
        RecordDependentRead(dst, src.var);
    }
}

void DeadStoreEliminationOptimizerPass::RecordWrite(VariableArg dst, IROp *op) {
    if (!dst.var.IsPresent()) {
        return;
    }
    auto varIndex = dst.var.Index();
    ResizeWrites(varIndex);
    m_varWrites[varIndex].op = op;
    m_varWrites[varIndex].read = false;
    m_varWrites[varIndex].consumed = false;
}

void DeadStoreEliminationOptimizerPass::ResizeWrites(size_t index) {
    if (m_varWrites.size() <= index) {
        m_varWrites.resize(index + 1);
    }
}

void DeadStoreEliminationOptimizerPass::ResizeDependencies(size_t index) {
    if (m_dependencies.size() <= index) {
        m_dependencies.resize(index + 1);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// GPR read and write tracking

void DeadStoreEliminationOptimizerPass::RecordGPRRead(GPRArg gpr) {
    m_gprWrites[gpr.Index()] = nullptr; // Leave instruction alone
}

void DeadStoreEliminationOptimizerPass::RecordGPRWrite(GPRArg gpr, IROp *op) {
    auto gprIndex = gpr.Index();
    IROp *writeOp = m_gprWrites[gprIndex];
    if (writeOp != nullptr) {
        // GPR is overwritten
        // Erase previous instruction, which is always going to be an IRSetRegisterOp
        m_emitter.Erase(writeOp);
    }
    m_gprWrites[gprIndex] = op;
}

// ---------------------------------------------------------------------------------------------------------------------
// PSR read and write tracking

bool DeadStoreEliminationOptimizerPass::RecordAndEraseDeadCPSRRead(VariableArg var, IROp *loadOp) {
    if (!var.var.IsPresent()) {
        return false;
    }

    // Assign variable to current CPSR version
    const auto index = m_cpsrVersion - 1; // CPSR version is 1-indexed
    ResizeCPSRToVarMap(index);
    if (!m_cpsrVarMap[index].var.IsPresent()) {
        m_cpsrVarMap[index].var = var.var;
    }

    // Assign CPSR version to the variable
    const auto varIndex = var.var.Index();
    ResizeVarToCPSRVersionMap(varIndex);
    m_varCPSRVersionMap[varIndex] = m_cpsrVersion;

    return CheckAndEraseDeadCPSRLoadStore(loadOp);
}

void DeadStoreEliminationOptimizerPass::RecordCPSRWrite(VariableArg src, IROp *op) {
    if (!src.var.IsPresent()) {
        return;
    }

    // Update CPSR version to that of the variable, if present
    const auto varIndex = src.var.Index();
    if (varIndex < m_varCPSRVersionMap.size() && m_varCPSRVersionMap[varIndex] != 0) {
        m_cpsrVersion = m_varCPSRVersionMap[varIndex];
        m_nextCPSRVersion = m_cpsrVersion + 1;

        // Associate this version with the given write op
        const auto index = m_cpsrVersion - 1; // CPSR version is 1-indexed
        assert(index < m_cpsrVarMap.size());  // this entry should exist
        m_cpsrVarMap[index].writeOp = op;
    } else {
        // Increment CPSR to the next CPSR version
        m_cpsrVersion = m_nextCPSRVersion++;
    }
}

bool DeadStoreEliminationOptimizerPass::CheckAndEraseDeadCPSRLoadStore(IROp *loadOp) {
    const auto versionIndex = m_cpsrVersion - 1; // CPSR version is 1-indexed
    if (versionIndex >= m_cpsrVarMap.size()) {
        return false;
    }

    // If the current version of CPSR comes from a previous store without modifications, erase both instructions
    auto &entry = m_cpsrVarMap[versionIndex];
    if (!entry.var.IsPresent() || entry.writeOp == nullptr) {
        return false;
    }

    m_emitter.Erase(loadOp);
    m_emitter.Erase(entry.writeOp);
    entry.writeOp = nullptr;
    return true;
}

bool DeadStoreEliminationOptimizerPass::HasCPSRVersion(VariableArg var) {
    if (!var.var.IsPresent()) {
        return false;
    }

    const auto varIndex = var.var.Index();
    if (varIndex < m_varCPSRVersionMap.size()) {
        return m_varCPSRVersionMap[varIndex] != 0;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::HasCPSRVersion(VarOrImmArg var) {
    if (var.immediate) {
        return false;
    }
    return HasCPSRVersion(var.var);
}

void DeadStoreEliminationOptimizerPass::AssignNewCPSRVersion(VariableArg var) {
    if (!var.var.IsPresent()) {
        return;
    }

    const auto varIndex = var.var.Index();
    ResizeVarToCPSRVersionMap(varIndex);
    m_varCPSRVersionMap[varIndex] = m_nextCPSRVersion++;

    const auto versionIndex = m_varCPSRVersionMap[varIndex] - 1;
    ResizeCPSRToVarMap(versionIndex);
    m_cpsrVarMap[versionIndex].var = var.var;
}

void DeadStoreEliminationOptimizerPass::CopyCPSRVersion(VariableArg dst, VariableArg src) {
    if (!dst.var.IsPresent() || !src.var.IsPresent()) {
        return;
    }

    const auto srcIndex = src.var.Index();
    if (srcIndex >= m_varCPSRVersionMap.size()) {
        return;
    }
    if (m_varCPSRVersionMap[srcIndex] == 0) {
        return;
    }

    const auto dstIndex = dst.var.Index();
    ResizeVarToCPSRVersionMap(dstIndex);
    m_varCPSRVersionMap[dstIndex] = m_varCPSRVersionMap[srcIndex];

    const auto versionIndex = m_varCPSRVersionMap[dstIndex] - 1;
    ResizeCPSRToVarMap(versionIndex);
    m_cpsrVarMap[versionIndex].var = dst.var;
}

void DeadStoreEliminationOptimizerPass::SubstituteCPSRVar(VariableArg &var) {
    if (!var.var.IsPresent()) {
        return;
    }

    // Check if there is a CPSR version associated with the variable
    const auto varIndex = var.var.Index();
    if (varIndex >= m_varCPSRVersionMap.size()) {
        return;
    }
    const auto version = m_varCPSRVersionMap[varIndex];
    if (version == 0) {
        return;
    }

    // Replace variable with the one corresponding to this version, if present
    const auto versionIndex = version - 1;
    if (versionIndex >= m_cpsrVarMap.size()) {
        return;
    }
    auto &entry = m_cpsrVarMap[versionIndex];
    if (entry.var.IsPresent()) {
        MarkDirty(var != entry.var);
        var = entry.var;
    }
}

void DeadStoreEliminationOptimizerPass::SubstituteCPSRVar(VarOrImmArg &var) {
    if (var.immediate) {
        return;
    }
    SubstituteCPSRVar(var.var);
}

void DeadStoreEliminationOptimizerPass::ResizeCPSRToVarMap(size_t index) {
    if (m_cpsrVarMap.size() <= index) {
        m_cpsrVarMap.resize(index + 1);
    }
}

void DeadStoreEliminationOptimizerPass::ResizeVarToCPSRVersionMap(size_t index) {
    if (m_varCPSRVersionMap.size() <= index) {
        m_varCPSRVersionMap.resize(index + 1);
    }
}

void DeadStoreEliminationOptimizerPass::RecordSPSRRead(arm::Mode mode) {
    m_spsrWrites[static_cast<size_t>(mode)] = nullptr; // Leave instruction alone
}

void DeadStoreEliminationOptimizerPass::RecordSPSRWrite(arm::Mode mode, IROp *op) {
    auto spsrIndex = static_cast<size_t>(mode);
    IROp *writeOp = m_spsrWrites[spsrIndex];
    if (writeOp != nullptr) {
        // SPSR for the given mode is overwritten
        // Erase previous instruction, which is always going to be an IRSetSPSROp
        m_emitter.Erase(writeOp);
    }
    m_spsrWrites[spsrIndex] = op;
}

// ---------------------------------------------------------------------------------------------------------------------
// Host flag writes tracking

void DeadStoreEliminationOptimizerPass::RecordHostFlagsRead(arm::Flags flags) {
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

void DeadStoreEliminationOptimizerPass::RecordHostFlagsWrite(arm::Flags flags, IROp *op) {
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
// Flags tracking

void DeadStoreEliminationOptimizerPass::ResizeFlagWritesPerVar(size_t index) {
    if (m_flagWritesPerVar.size() <= index) {
        m_flagWritesPerVar.resize(index + 1);
    }
}

void DeadStoreEliminationOptimizerPass::InitFlagWrites(VariableArg base) {
    if (!base.var.IsPresent()) {
        return;
    }

    const auto varIndex = base.var.Index();
    ResizeFlagWritesPerVar(varIndex);
    m_flagWritesPerVar[varIndex].base = base.var;
}

void DeadStoreEliminationOptimizerPass::RecordFlagWrites(VariableArg dst, VariableArg src, arm::Flags flags,
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
// Generic EraseWrite for variables

void DeadStoreEliminationOptimizerPass::EraseWriteRecursive(Variable var, IROp *op) {
    if (!var.IsPresent()) {
        return;
    }

    bool erased = VisitIROp(op, [this, var](auto op) { return EraseWrite(var, op); });

    // Follow dependencies
    if (erased && var.Index() < m_dependencies.size()) {
        for (auto &dep : m_dependencies[var.Index()]) {
            if (dep.IsPresent()) {
                auto &write = m_varWrites[dep.Index()];
                if (write.op != nullptr && !write.consumed) {
                    EraseWriteRecursive(dep, write.op);
                }
            }
        }
    }
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRGetRegisterOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRGetCPSROp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRGetSPSROp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRMemReadOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRLogicalShiftLeftOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRLogicalShiftRightOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRArithmeticShiftRightOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRRotateRightOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRRotateRightExtendedOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRBitwiseAndOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRBitwiseOrOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRBitwiseXorOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRBitClearOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRCountLeadingZerosOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRAddOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRAddCarryOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRSubtractOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRSubtractCarryOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRMoveOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRMoveNegatedOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRSaturatingAddOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRSaturatingSubtractOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRMultiplyOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRMultiplyLongOp *op) {
    if (op->dstLo == var) {
        MarkDirty();
        op->dstLo.var = {};
    }
    if (op->dstHi == var) {
        MarkDirty();
        op->dstHi.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRAddLongOp *op) {
    if (op->dstLo == var) {
        MarkDirty();
        op->dstLo.var = {};
    }
    if (op->dstHi == var) {
        MarkDirty();
        op->dstHi.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRLoadFlagsOp *op) {
    if (op->dstCPSR == var) {
        MarkDirty();
        op->dstCPSR.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRLoadStickyOverflowOp *op) {
    if (op->dstCPSR == var) {
        MarkDirty();
        op->dstCPSR.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRLoadCopRegisterOp *op) {
    if (op->dstValue == var) {
        MarkDirty();
        op->dstValue.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRConstantOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRCopyVarOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRGetBaseVectorAddressOp *op) {
    if (op->dst == var) {
        MarkDirty();
        op->dst.var = {};
    }
    return EraseDeadInstruction(op);
}

// ---------------------------------------------------------------------------------------------------------------------
// Generic EraseFlagWrite

void DeadStoreEliminationOptimizerPass::EraseFlagWrite(arm::Flags flag, IRBitwiseAndOp *op) {
    if (auto split = SplitImmVarArgPair(op->lhs, op->rhs)) {
        auto &[imm, _] = *split;
        MarkDirty(imm.value & ~static_cast<uint32_t>(flag));
        imm.value |= static_cast<uint32_t>(flag);
    }
}

void DeadStoreEliminationOptimizerPass::EraseFlagWrite(arm::Flags flag, IRBitwiseOrOp *op) {
    if (auto split = SplitImmVarArgPair(op->lhs, op->rhs)) {
        auto &[imm, _] = *split;
        MarkDirty(imm.value & static_cast<uint32_t>(flag));
        imm.value &= ~static_cast<uint32_t>(flag);
    }
}

void DeadStoreEliminationOptimizerPass::EraseFlagWrite(arm::Flags flag, IRBitClearOp *op) {
    if (auto split = SplitImmVarArgPair(op->lhs, op->rhs)) {
        auto &[imm, _] = *split;
        MarkDirty(imm.value & static_cast<uint32_t>(flag));
        imm.value &= ~static_cast<uint32_t>(flag);
    }
}

void DeadStoreEliminationOptimizerPass::EraseFlagWrite(arm::Flags flag, IRLoadFlagsOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
}

void DeadStoreEliminationOptimizerPass::EraseFlagWrite(arm::Flags flag, IRLoadStickyOverflowOp *op) {
    if (op->setQ && BitmaskEnum(flag).AnyOf(arm::Flags::Q)) {
        op->setQ = false;
        MarkDirty();
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Generic EraseHostFlagWrite

bool DeadStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRLogicalShiftLeftOp *op) {
    if (BitmaskEnum(flag).AnyOf(arm::Flags::C)) {
        MarkDirty();
        op->setCarry = false;
    }
    return !op->setCarry;
}

bool DeadStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRLogicalShiftRightOp *op) {
    if (BitmaskEnum(flag).AnyOf(arm::Flags::C)) {
        MarkDirty();
        op->setCarry = false;
    }
    return !op->setCarry;
}

bool DeadStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRArithmeticShiftRightOp *op) {
    if (BitmaskEnum(flag).AnyOf(arm::Flags::C)) {
        MarkDirty();
        op->setCarry = false;
    }
    return !op->setCarry;
}

bool DeadStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRRotateRightOp *op) {
    if (BitmaskEnum(flag).AnyOf(arm::Flags::C)) {
        MarkDirty();
        op->setCarry = false;
    }
    return !op->setCarry;
}

bool DeadStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRRotateRightExtendedOp *op) {
    if (BitmaskEnum(flag).AnyOf(arm::Flags::C)) {
        MarkDirty();
        op->setCarry = false;
    }
    return !op->setCarry;
}

bool DeadStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRBitwiseAndOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
    return op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRBitwiseOrOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
    return op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRBitwiseXorOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
    return op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRBitClearOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
    return op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRAddOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
    return op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRAddCarryOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
    return op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRSubtractOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
    return op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRSubtractCarryOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
    return op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRMoveOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
    return op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRMoveNegatedOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
    return op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRSaturatingAddOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
    return op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRSaturatingSubtractOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
    return op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRMultiplyOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
    return op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRMultiplyLongOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
    return op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRAddLongOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
    return op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRStoreFlagsOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
    return op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRLoadFlagsOp *op) {
    MarkDirty((op->flags & flag) != arm::Flags::None);
    op->flags &= ~flag;
    return op->flags == arm::Flags::None;
}

bool DeadStoreEliminationOptimizerPass::EraseHostFlagWrite(arm::Flags flag, IRLoadStickyOverflowOp *op) {
    if (op->setQ && BitmaskEnum(flag).AnyOf(arm::Flags::Q)) {
        MarkDirty();
        op->setQ = false;
    }
    return !op->setQ;
}

// ---------------------------------------------------------------------------------------------------------------------
// Generic EraseDeadInstruction

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRGetRegisterOp *op) {
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRSetRegisterOp *op) {
    // TODO: implement
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRGetCPSROp *op) {
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRSetCPSROp *op) {
    // TODO: implement
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRGetSPSROp *op) {
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRSetSPSROp *op) {
    // TODO: implement
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRMemReadOp *op) {
    if (!op->dst.var.IsPresent()) {
        if (op->address.immediate && false /* TODO: no side effects on address */) {
            m_emitter.Erase(op);
            return true;
        }
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRLogicalShiftLeftOp *op) {
    if (!op->dst.var.IsPresent() && !op->setCarry) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRLogicalShiftRightOp *op) {
    if (!op->dst.var.IsPresent() && !op->setCarry) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRArithmeticShiftRightOp *op) {
    if (!op->dst.var.IsPresent() && !op->setCarry) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRRotateRightOp *op) {
    if (!op->dst.var.IsPresent() && !op->setCarry) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRRotateRightExtendedOp *op) {
    if (!op->dst.var.IsPresent() && !op->setCarry) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRBitwiseAndOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRBitwiseOrOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRBitwiseXorOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRBitClearOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRCountLeadingZerosOp *op) {
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRAddOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRAddCarryOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRSubtractOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRSubtractCarryOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRMoveOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRMoveNegatedOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRSaturatingAddOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRSaturatingSubtractOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRMultiplyOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRMultiplyLongOp *op) {
    if (!op->dstLo.var.IsPresent() && !op->dstHi.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRAddLongOp *op) {
    if (!op->dstLo.var.IsPresent() && !op->dstHi.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRStoreFlagsOp *op) {
    if (op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRLoadFlagsOp *op) {
    if (!op->dstCPSR.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRLoadStickyOverflowOp *op) {
    if (!op->dstCPSR.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRLoadCopRegisterOp *op) {
    if (!op->dstValue.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRConstantOp *op) {
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRCopyVarOp *op) {
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseDeadInstruction(IRGetBaseVectorAddressOp *op) {
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

} // namespace armajitto::ir
