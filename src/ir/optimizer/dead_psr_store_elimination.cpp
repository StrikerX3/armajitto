#include "dead_psr_store_elimination.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

#include <cassert>

namespace armajitto::ir {

DeadPSRStoreEliminationOptimizerPass::DeadPSRStoreEliminationOptimizerPass(Emitter &emitter)
    : DeadStoreEliminationOptimizerPassBase(emitter) {

    const uint32_t varCount = emitter.VariableCount();
    m_cpsrVarMap.resize(varCount);
    m_varCPSRVersionMap.resize(varCount);
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRGetCPSROp *op) {
    if (RecordAndEraseDeadCPSRRead(op->dst, op)) {
        return;
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRSetCPSROp *op) {
    if (!op->src.immediate) {
        RecordCPSRWrite(op->src.var, op);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRGetSPSROp *op) {
    RecordSPSRRead(op->mode);
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRSetSPSROp *op) {
    RecordSPSRWrite(op->mode, op);
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRLogicalShiftLeftOp *op) {
    if (HasCPSRVersion(op->value)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    if (HasCPSRVersion(op->value)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    if (HasCPSRVersion(op->value)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRRotateRightOp *op) {
    if (HasCPSRVersion(op->value)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRRotateRightExtendedOp *op) {
    if (HasCPSRVersion(op->value)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRBitwiseAndOp *op) {
    if (HasCPSRVersion(op->lhs) || HasCPSRVersion(op->rhs)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRBitwiseOrOp *op) {
    if (HasCPSRVersion(op->lhs) || HasCPSRVersion(op->rhs)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRBitwiseXorOp *op) {
    if (HasCPSRVersion(op->lhs) || HasCPSRVersion(op->rhs)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRBitClearOp *op) {
    if (HasCPSRVersion(op->lhs) || HasCPSRVersion(op->rhs)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRCountLeadingZerosOp *op) {
    if (HasCPSRVersion(op->value)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRAddOp *op) {
    if (HasCPSRVersion(op->lhs) || HasCPSRVersion(op->rhs)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRAddCarryOp *op) {
    if (HasCPSRVersion(op->lhs) || HasCPSRVersion(op->rhs)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRSubtractOp *op) {
    if (HasCPSRVersion(op->lhs) || HasCPSRVersion(op->rhs)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRSubtractCarryOp *op) {
    if (HasCPSRVersion(op->lhs) || HasCPSRVersion(op->rhs)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRMoveOp *op) {
    if (!op->value.immediate) {
        CopyCPSRVersion(op->dst, op->value.var);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRMoveNegatedOp *op) {
    if (HasCPSRVersion(op->value)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRSaturatingAddOp *op) {
    if (HasCPSRVersion(op->lhs) || HasCPSRVersion(op->rhs)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    if (HasCPSRVersion(op->lhs) || HasCPSRVersion(op->rhs)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRMultiplyOp *op) {
    if (HasCPSRVersion(op->lhs) || HasCPSRVersion(op->rhs)) {
        AssignNewCPSRVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRMultiplyLongOp *op) {
    if (HasCPSRVersion(op->lhs) || HasCPSRVersion(op->rhs)) {
        AssignNewCPSRVersion(op->dstLo);
        AssignNewCPSRVersion(op->dstHi);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRAddLongOp *op) {
    if (HasCPSRVersion(op->lhsLo) || HasCPSRVersion(op->lhsHi) || HasCPSRVersion(op->rhsLo) ||
        HasCPSRVersion(op->rhsHi)) {
        AssignNewCPSRVersion(op->dstLo);
        AssignNewCPSRVersion(op->dstHi);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRLoadFlagsOp *op) {
    if (HasCPSRVersion(op->srcCPSR)) {
        AssignNewCPSRVersion(op->dstCPSR);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRLoadStickyOverflowOp *op) {
    if (HasCPSRVersion(op->srcCPSR)) {
        AssignNewCPSRVersion(op->dstCPSR);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRCopyVarOp *op) {
    CopyCPSRVersion(op->dst, op->var);
}

// ---------------------------------------------------------------------------------------------------------------------
// PSR read and write tracking

bool DeadPSRStoreEliminationOptimizerPass::RecordAndEraseDeadCPSRRead(VariableArg var, IROp *loadOp) {
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

void DeadPSRStoreEliminationOptimizerPass::RecordCPSRWrite(VariableArg src, IROp *op) {
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

bool DeadPSRStoreEliminationOptimizerPass::CheckAndEraseDeadCPSRLoadStore(IROp *loadOp) {
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

bool DeadPSRStoreEliminationOptimizerPass::HasCPSRVersion(VariableArg var) {
    if (!var.var.IsPresent()) {
        return false;
    }

    const auto varIndex = var.var.Index();
    if (varIndex < m_varCPSRVersionMap.size()) {
        return m_varCPSRVersionMap[varIndex] != 0;
    }
    return false;
}

bool DeadPSRStoreEliminationOptimizerPass::HasCPSRVersion(VarOrImmArg var) {
    if (var.immediate) {
        return false;
    }
    return HasCPSRVersion(var.var);
}

void DeadPSRStoreEliminationOptimizerPass::AssignNewCPSRVersion(VariableArg var) {
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

void DeadPSRStoreEliminationOptimizerPass::CopyCPSRVersion(VariableArg dst, VariableArg src) {
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

void DeadPSRStoreEliminationOptimizerPass::SubstituteCPSRVar(VariableArg &var) {
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

void DeadPSRStoreEliminationOptimizerPass::SubstituteCPSRVar(VarOrImmArg &var) {
    if (var.immediate) {
        return;
    }
    SubstituteCPSRVar(var.var);
}

void DeadPSRStoreEliminationOptimizerPass::ResizeCPSRToVarMap(size_t index) {
    if (m_cpsrVarMap.size() <= index) {
        m_cpsrVarMap.resize(index + 1);
    }
}

void DeadPSRStoreEliminationOptimizerPass::ResizeVarToCPSRVersionMap(size_t index) {
    if (m_varCPSRVersionMap.size() <= index) {
        m_varCPSRVersionMap.resize(index + 1);
    }
}

void DeadPSRStoreEliminationOptimizerPass::RecordSPSRRead(arm::Mode mode) {
    m_spsrWrites[static_cast<size_t>(mode)] = nullptr; // Leave instruction alone
}

void DeadPSRStoreEliminationOptimizerPass::RecordSPSRWrite(arm::Mode mode, IROp *op) {
    auto spsrIndex = static_cast<size_t>(mode);
    IROp *writeOp = m_spsrWrites[spsrIndex];
    if (writeOp != nullptr) {
        // SPSR for the given mode is overwritten
        // Erase previous instruction, which is always going to be an IRSetSPSROp
        m_emitter.Erase(writeOp);
    }
    m_spsrWrites[spsrIndex] = op;
}

} // namespace armajitto::ir
