#include "dead_psr_store_elimination.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

#include <cassert>

namespace armajitto::ir {

DeadPSRStoreEliminationOptimizerPass::DeadPSRStoreEliminationOptimizerPass(Emitter &emitter)
    : DeadStoreEliminationOptimizerPassBase(emitter) {

    const uint32_t varCount = emitter.VariableCount();
    m_psrToVarMap.resize(varCount);
    m_varToPSRVersionMap.resize(varCount);

    m_nextPSRVersion = 1;
    for (auto &ver : m_psrVersions) {
        ver = m_nextPSRVersion++;
    }
    m_psrWrites.fill(nullptr);
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRSetRegisterOp *op) {
    SubstituteVar(op->src);
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRGetCPSROp *op) {
    RecordCPSRRead(op->dst, op);
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRSetCPSROp *op) {
    SubstituteVar(op->src);
    if (!op->src.immediate) {
        RecordCPSRWrite(op->src.var, op);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRGetSPSROp *op) {
    RecordSPSRRead(op->mode, op->dst, op);
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRSetSPSROp *op) {
    SubstituteVar(op->src);
    if (!op->src.immediate) {
        RecordSPSRWrite(op->mode, op->src.var, op);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRMemReadOp *op) {
    SubstituteVar(op->address);
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRMemWriteOp *op) {
    SubstituteVar(op->src);
    SubstituteVar(op->address);
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRPreloadOp *op) {
    SubstituteVar(op->address);
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRLogicalShiftLeftOp *op) {
    SubstituteVar(op->value);
    SubstituteVar(op->amount);
    if (HasVersion(op->value)) {
        AssignNewVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    SubstituteVar(op->value);
    SubstituteVar(op->amount);
    if (HasVersion(op->value)) {
        AssignNewVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    SubstituteVar(op->value);
    SubstituteVar(op->amount);
    if (HasVersion(op->value)) {
        AssignNewVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRRotateRightOp *op) {
    SubstituteVar(op->value);
    SubstituteVar(op->amount);
    if (HasVersion(op->value)) {
        AssignNewVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRRotateRightExtendedOp *op) {
    SubstituteVar(op->value);
    if (HasVersion(op->value)) {
        AssignNewVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRBitwiseAndOp *op) {
    SubstituteVar(op->lhs);
    SubstituteVar(op->rhs);
    if (HasVersion(op->lhs) || HasVersion(op->rhs)) {
        AssignNewVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRBitwiseOrOp *op) {
    SubstituteVar(op->lhs);
    SubstituteVar(op->rhs);
    if (HasVersion(op->lhs) || HasVersion(op->rhs)) {
        AssignNewVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRBitwiseXorOp *op) {
    SubstituteVar(op->lhs);
    SubstituteVar(op->rhs);
    if (HasVersion(op->lhs) || HasVersion(op->rhs)) {
        AssignNewVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRBitClearOp *op) {
    SubstituteVar(op->lhs);
    SubstituteVar(op->rhs);
    if (HasVersion(op->lhs) || HasVersion(op->rhs)) {
        AssignNewVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRCountLeadingZerosOp *op) {
    SubstituteVar(op->value);
    if (HasVersion(op->value)) {
        AssignNewVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRAddOp *op) {
    SubstituteVar(op->lhs);
    SubstituteVar(op->rhs);
    if (HasVersion(op->lhs) || HasVersion(op->rhs)) {
        AssignNewVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRAddCarryOp *op) {
    SubstituteVar(op->lhs);
    SubstituteVar(op->rhs);
    if (HasVersion(op->lhs) || HasVersion(op->rhs)) {
        AssignNewVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRSubtractOp *op) {
    SubstituteVar(op->lhs);
    SubstituteVar(op->rhs);
    if (HasVersion(op->lhs) || HasVersion(op->rhs)) {
        AssignNewVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRSubtractCarryOp *op) {
    SubstituteVar(op->lhs);
    SubstituteVar(op->rhs);
    if (HasVersion(op->lhs) || HasVersion(op->rhs)) {
        AssignNewVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRMoveOp *op) {
    SubstituteVar(op->value);
    if (!op->value.immediate) {
        CopyVersion(op->dst, op->value.var);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRMoveNegatedOp *op) {
    SubstituteVar(op->value);
    if (HasVersion(op->value)) {
        AssignNewVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRSaturatingAddOp *op) {
    SubstituteVar(op->lhs);
    SubstituteVar(op->rhs);
    if (HasVersion(op->lhs) || HasVersion(op->rhs)) {
        AssignNewVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    SubstituteVar(op->lhs);
    SubstituteVar(op->rhs);
    if (HasVersion(op->lhs) || HasVersion(op->rhs)) {
        AssignNewVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRMultiplyOp *op) {
    SubstituteVar(op->lhs);
    SubstituteVar(op->rhs);
    if (HasVersion(op->lhs) || HasVersion(op->rhs)) {
        AssignNewVersion(op->dst);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRMultiplyLongOp *op) {
    SubstituteVar(op->lhs);
    SubstituteVar(op->rhs);
    if (HasVersion(op->lhs) || HasVersion(op->rhs)) {
        AssignNewVersion(op->dstLo);
        AssignNewVersion(op->dstHi);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRAddLongOp *op) {
    SubstituteVar(op->lhsLo);
    SubstituteVar(op->lhsHi);
    SubstituteVar(op->rhsLo);
    SubstituteVar(op->rhsHi);
    if (HasVersion(op->lhsLo) || HasVersion(op->lhsHi) || HasVersion(op->rhsLo) || HasVersion(op->rhsHi)) {
        AssignNewVersion(op->dstLo);
        AssignNewVersion(op->dstHi);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRLoadFlagsOp *op) {
    SubstituteVar(op->srcCPSR);
    if (HasVersion(op->srcCPSR)) {
        AssignNewVersion(op->dstCPSR);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRLoadStickyOverflowOp *op) {
    SubstituteVar(op->srcCPSR);
    if (HasVersion(op->srcCPSR)) {
        AssignNewVersion(op->dstCPSR);
    }
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRBranchOp *op) {
    SubstituteVar(op->address);
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRBranchExchangeOp *op) {
    SubstituteVar(op->address);
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRStoreCopRegisterOp *op) {
    SubstituteVar(op->srcValue);
}

void DeadPSRStoreEliminationOptimizerPass::Process(IRCopyVarOp *op) {
    SubstituteVar(op->var);
    CopyVersion(op->dst, op->var);
}

// ---------------------------------------------------------------------------------------------------------------------
// PSR read and write tracking

void DeadPSRStoreEliminationOptimizerPass::RecordCPSRRead(VariableArg var, IROp *loadOp) {
    RecordPSRRead(0, var, loadOp);
}

void DeadPSRStoreEliminationOptimizerPass::RecordCPSRWrite(VariableArg src, IROp *op) {
    RecordPSRWrite(0, src, op);
}

void DeadPSRStoreEliminationOptimizerPass::RecordSPSRRead(arm::Mode mode, VariableArg var, IROp *loadOp) {
    RecordPSRRead(SPSRIndex(mode), var, loadOp);
}

void DeadPSRStoreEliminationOptimizerPass::RecordSPSRWrite(arm::Mode mode, VariableArg src, IROp *op) {
    RecordPSRWrite(SPSRIndex(mode), src, op);
}

void DeadPSRStoreEliminationOptimizerPass::RecordPSRRead(size_t index, VariableArg var, IROp *loadOp) {
    m_psrWrites[index] = nullptr; // Leave previous write instruction alone

    if (!var.var.IsPresent()) {
        return;
    }

    // Assign variable to current PSR version
    auto &psrVersion = m_psrVersions[index];
    const auto versionIndex = psrVersion - 1; // PSR versions are 1-indexed
    ResizePSRToVarMap(versionIndex);
    auto &versionEntry = m_psrToVarMap[versionIndex];
    if (!versionEntry.var.IsPresent()) {
        versionEntry.var = var.var;
    }

    // Assign PSR version to the variable
    const auto varIndex = var.var.Index();
    ResizeVarToPSRVersionMap(varIndex);
    m_varToPSRVersionMap[varIndex] = psrVersion;

    // If the current version of the PSR comes from a previous store without modifications, erase both instructions
    if (versionEntry.writeOp == nullptr) {
        return;
    }

    m_emitter.Erase(loadOp);
    m_emitter.Erase(versionEntry.writeOp);
    versionEntry.writeOp = nullptr;
}

void DeadPSRStoreEliminationOptimizerPass::RecordPSRWrite(size_t index, VariableArg src, IROp *op) {
    auto *psrWrite = m_psrWrites[index];
    if (psrWrite != nullptr) {
        // PSR is overwritten; erase previous write instruction
        m_emitter.Erase(psrWrite);
    }
    psrWrite = op;

    if (!src.var.IsPresent()) {
        return;
    }

    // Update PSR version to that of the variable, if present
    auto &psrVersion = m_psrVersions[index];
    const auto varIndex = src.var.Index();
    if (varIndex < m_varToPSRVersionMap.size() && m_varToPSRVersionMap[varIndex] != 0) {
        auto version = m_varToPSRVersionMap[varIndex];
        if (version == psrVersion) {
            // No changes were made; erase this write
            m_emitter.Erase(op);
        }
        psrVersion = version;

        // Associate this version with the given write op
        const auto index = psrVersion - 1;    // PSR versions are 1-indexed
        assert(index < m_psrToVarMap.size()); // this entry should exist
        m_psrToVarMap[index].writeOp = op;
    } else {
        // Set PSR to the next PSR version
        psrVersion = m_nextPSRVersion;
    }
    ++m_nextPSRVersion;
}

bool DeadPSRStoreEliminationOptimizerPass::HasVersion(VariableArg var) {
    if (!var.var.IsPresent()) {
        return false;
    }

    const auto varIndex = var.var.Index();
    if (varIndex < m_varToPSRVersionMap.size()) {
        return m_varToPSRVersionMap[varIndex] != 0;
    }
    return false;
}

bool DeadPSRStoreEliminationOptimizerPass::HasVersion(VarOrImmArg var) {
    if (var.immediate) {
        return false;
    }
    return HasVersion(var.var);
}

void DeadPSRStoreEliminationOptimizerPass::AssignNewVersion(VariableArg var) {
    if (!var.var.IsPresent()) {
        return;
    }

    const auto varIndex = var.var.Index();
    ResizeVarToPSRVersionMap(varIndex);
    m_varToPSRVersionMap[varIndex] = m_nextPSRVersion++;

    const auto versionIndex = m_varToPSRVersionMap[varIndex] - 1;
    ResizePSRToVarMap(versionIndex);
    m_psrToVarMap[versionIndex].var = var.var;
}

void DeadPSRStoreEliminationOptimizerPass::CopyVersion(VariableArg dst, VariableArg src) {
    if (!dst.var.IsPresent() || !src.var.IsPresent()) {
        return;
    }

    const auto srcIndex = src.var.Index();
    if (srcIndex >= m_varToPSRVersionMap.size()) {
        return;
    }
    if (m_varToPSRVersionMap[srcIndex] == 0) {
        return;
    }

    const auto dstIndex = dst.var.Index();
    ResizeVarToPSRVersionMap(dstIndex);
    m_varToPSRVersionMap[dstIndex] = m_varToPSRVersionMap[srcIndex];

    const auto versionIndex = m_varToPSRVersionMap[dstIndex] - 1;
    ResizePSRToVarMap(versionIndex);
    m_psrToVarMap[versionIndex].var = dst.var;
}

void DeadPSRStoreEliminationOptimizerPass::SubstituteVar(VariableArg &var) {
    if (!var.var.IsPresent()) {
        return;
    }

    // Check if there is a PSR version associated with the variable
    const auto varIndex = var.var.Index();
    if (varIndex >= m_varToPSRVersionMap.size()) {
        return;
    }
    const auto version = m_varToPSRVersionMap[varIndex];
    if (version == 0) {
        return;
    }

    // Replace variable with the one corresponding to this version, if present
    const auto versionIndex = version - 1;
    if (versionIndex >= m_psrToVarMap.size()) {
        return;
    }
    auto &entry = m_psrToVarMap[versionIndex];
    if (entry.var.IsPresent()) {
        MarkDirty(var != entry.var);
        var = entry.var;
    }
}

void DeadPSRStoreEliminationOptimizerPass::SubstituteVar(VarOrImmArg &var) {
    if (var.immediate) {
        return;
    }
    SubstituteVar(var.var);
}

void DeadPSRStoreEliminationOptimizerPass::ResizePSRToVarMap(size_t index) {
    if (m_psrToVarMap.size() <= index) {
        m_psrToVarMap.resize(index + 1);
    }
}

void DeadPSRStoreEliminationOptimizerPass::ResizeVarToPSRVersionMap(size_t index) {
    if (m_varToPSRVersionMap.size() <= index) {
        m_varToPSRVersionMap.resize(index + 1);
    }
}

} // namespace armajitto::ir
