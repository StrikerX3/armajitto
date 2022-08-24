#include "dead_reg_store_elimination.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

#include <cassert>

namespace armajitto::ir {

DeadRegisterStoreEliminationOptimizerPass::DeadRegisterStoreEliminationOptimizerPass(Emitter &emitter)
    : DeadStoreEliminationOptimizerPassBase(emitter) {

    const uint32_t varCount = emitter.VariableCount();
    m_versionToVarMap.resize(varCount);
    m_varToVersionMap.resize(varCount);

    m_nextVersion = 1;
    for (auto &ver : m_psrVersions) {
        ver = m_nextVersion++;
    }
    m_psrWrites.fill(nullptr);

    for (auto &ver : m_gprVersions) {
        ver = m_nextVersion++;
    }
    m_gprWrites.fill(nullptr);
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRGetRegisterOp *op) {
    RecordGPRRead(op->src, op->dst, op);
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRSetRegisterOp *op) {
    SubstituteVar(op->src);
    if (!op->src.immediate) {
        RecordGPRWrite(op->dst, op->src.var, op);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRGetCPSROp *op) {
    RecordCPSRRead(op->dst, op);
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRSetCPSROp *op) {
    SubstituteVar(op->src);
    if (!op->src.immediate) {
        RecordCPSRWrite(op->src.var, op);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRGetSPSROp *op) {
    RecordSPSRRead(op->mode, op->dst, op);
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRSetSPSROp *op) {
    SubstituteVar(op->src);
    if (!op->src.immediate) {
        RecordSPSRWrite(op->mode, op->src.var, op);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRMemReadOp *op) {
    SubstituteVar(op->address);
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRMemWriteOp *op) {
    SubstituteVar(op->src);
    SubstituteVar(op->address);
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRPreloadOp *op) {
    SubstituteVar(op->address);
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRLogicalShiftLeftOp *op) {
    SubstituteVar(op->value);
    SubstituteVar(op->amount);
    if (IsTagged(op->value)) {
        AssignNewVersion(op->dst);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    SubstituteVar(op->value);
    SubstituteVar(op->amount);
    if (IsTagged(op->value)) {
        AssignNewVersion(op->dst);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    SubstituteVar(op->value);
    SubstituteVar(op->amount);
    if (IsTagged(op->value)) {
        AssignNewVersion(op->dst);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRRotateRightOp *op) {
    SubstituteVar(op->value);
    SubstituteVar(op->amount);
    if (IsTagged(op->value)) {
        AssignNewVersion(op->dst);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRRotateRightExtendedOp *op) {
    SubstituteVar(op->value);
    if (IsTagged(op->value)) {
        AssignNewVersion(op->dst);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRBitwiseAndOp *op) {
    SubstituteVar(op->lhs);
    SubstituteVar(op->rhs);
    if (IsTagged(op->lhs) || IsTagged(op->rhs)) {
        AssignNewVersion(op->dst);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRBitwiseOrOp *op) {
    SubstituteVar(op->lhs);
    SubstituteVar(op->rhs);
    if (IsTagged(op->lhs) || IsTagged(op->rhs)) {
        AssignNewVersion(op->dst);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRBitwiseXorOp *op) {
    SubstituteVar(op->lhs);
    SubstituteVar(op->rhs);
    if (IsTagged(op->lhs) || IsTagged(op->rhs)) {
        AssignNewVersion(op->dst);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRBitClearOp *op) {
    SubstituteVar(op->lhs);
    SubstituteVar(op->rhs);
    if (IsTagged(op->lhs) || IsTagged(op->rhs)) {
        AssignNewVersion(op->dst);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRCountLeadingZerosOp *op) {
    SubstituteVar(op->value);
    if (IsTagged(op->value)) {
        AssignNewVersion(op->dst);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRAddOp *op) {
    SubstituteVar(op->lhs);
    SubstituteVar(op->rhs);
    if (IsTagged(op->lhs) || IsTagged(op->rhs)) {
        AssignNewVersion(op->dst);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRAddCarryOp *op) {
    SubstituteVar(op->lhs);
    SubstituteVar(op->rhs);
    if (IsTagged(op->lhs) || IsTagged(op->rhs)) {
        AssignNewVersion(op->dst);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRSubtractOp *op) {
    SubstituteVar(op->lhs);
    SubstituteVar(op->rhs);
    if (IsTagged(op->lhs) || IsTagged(op->rhs)) {
        AssignNewVersion(op->dst);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRSubtractCarryOp *op) {
    SubstituteVar(op->lhs);
    SubstituteVar(op->rhs);
    if (IsTagged(op->lhs) || IsTagged(op->rhs)) {
        AssignNewVersion(op->dst);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRMoveOp *op) {
    SubstituteVar(op->value);
    if (!op->value.immediate) {
        CopyVersion(op->dst, op->value.var);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRMoveNegatedOp *op) {
    SubstituteVar(op->value);
    if (IsTagged(op->value)) {
        AssignNewVersion(op->dst);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRSaturatingAddOp *op) {
    SubstituteVar(op->lhs);
    SubstituteVar(op->rhs);
    if (IsTagged(op->lhs) || IsTagged(op->rhs)) {
        AssignNewVersion(op->dst);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    SubstituteVar(op->lhs);
    SubstituteVar(op->rhs);
    if (IsTagged(op->lhs) || IsTagged(op->rhs)) {
        AssignNewVersion(op->dst);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRMultiplyOp *op) {
    SubstituteVar(op->lhs);
    SubstituteVar(op->rhs);
    if (IsTagged(op->lhs) || IsTagged(op->rhs)) {
        AssignNewVersion(op->dst);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRMultiplyLongOp *op) {
    SubstituteVar(op->lhs);
    SubstituteVar(op->rhs);
    if (IsTagged(op->lhs) || IsTagged(op->rhs)) {
        AssignNewVersion(op->dstLo);
        AssignNewVersion(op->dstHi);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRAddLongOp *op) {
    SubstituteVar(op->lhsLo);
    SubstituteVar(op->lhsHi);
    SubstituteVar(op->rhsLo);
    SubstituteVar(op->rhsHi);
    if (IsTagged(op->lhsLo) || IsTagged(op->lhsHi) || IsTagged(op->rhsLo) || IsTagged(op->rhsHi)) {
        AssignNewVersion(op->dstLo);
        AssignNewVersion(op->dstHi);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRLoadFlagsOp *op) {
    SubstituteVar(op->srcCPSR);
    if (IsTagged(op->srcCPSR)) {
        AssignNewVersion(op->dstCPSR);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRLoadStickyOverflowOp *op) {
    SubstituteVar(op->srcCPSR);
    if (IsTagged(op->srcCPSR)) {
        AssignNewVersion(op->dstCPSR);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRBranchOp *op) {
    SubstituteVar(op->address);
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRBranchExchangeOp *op) {
    SubstituteVar(op->address);
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRStoreCopRegisterOp *op) {
    SubstituteVar(op->srcValue);
}

void DeadRegisterStoreEliminationOptimizerPass::Process(IRCopyVarOp *op) {
    SubstituteVar(op->var);
    CopyVersion(op->dst, op->var);
}

// ---------------------------------------------------------------------------------------------------------------------
// PSR read and write tracking

void DeadRegisterStoreEliminationOptimizerPass::RecordGPRRead(GPRArg gpr, VariableArg var, IROp *loadOp) {
    const auto index = gpr.Index();
    m_gprWrites[index] = nullptr; // Leave previous write instruction alone

    if (!var.var.IsPresent()) {
        return;
    }

    // Assign variable to current GPR version
    auto &gprVersion = m_gprVersions[index];
    const auto versionIndex = gprVersion - 1; // Versions are 1-indexed
    ResizeVersionToVarMap(versionIndex);
    auto &versionEntry = m_versionToVarMap[versionIndex];
    if (!versionEntry.var.IsPresent()) {
        versionEntry.var = var.var;
    }

    // Assign PSR version to the variable
    const auto varIndex = var.var.Index();
    ResizeVarToVersionMap(varIndex);
    m_varToVersionMap[varIndex] = gprVersion;

    // If the current version of the GPR comes from a previous store without modifications, erase both instructions
    if (versionEntry.writeOp != nullptr) {
        m_emitter.Erase(loadOp);
        m_emitter.Erase(versionEntry.writeOp);
        versionEntry.writeOp = nullptr;
    }
}

void DeadRegisterStoreEliminationOptimizerPass::RecordGPRWrite(GPRArg gpr, VariableArg src, IROp *op) {
    const auto index = gpr.Index();
    auto *gprWrite = m_gprWrites[index];
    if (gprWrite != nullptr) {
        // GPR is overwritten; erase previous write instruction
        m_emitter.Erase(gprWrite);
    }
    gprWrite = op;

    if (!src.var.IsPresent()) {
        return;
    }

    // Update GPR version to that of the variable, if present
    auto &gprVersion = m_gprVersions[index];
    const auto varIndex = src.var.Index();
    if (varIndex < m_varToVersionMap.size() && m_varToVersionMap[varIndex] != 0) {
        auto version = m_varToVersionMap[varIndex];
        if (version == gprVersion) {
            // No changes were made; erase this write
            m_emitter.Erase(op);
        }
        gprVersion = version;

        // Associate this version with the given write op
        const auto index = gprVersion - 1;        // Versions are 1-indexed
        assert(index < m_versionToVarMap.size()); // this entry should exist
        m_versionToVarMap[index].writeOp = op;
    } else {
        // Set PSR to the next version
        gprVersion = m_nextVersion;
    }
    ++m_nextVersion;
}

void DeadRegisterStoreEliminationOptimizerPass::RecordCPSRRead(VariableArg var, IROp *loadOp) {
    RecordPSRRead(0, var, loadOp);
}

void DeadRegisterStoreEliminationOptimizerPass::RecordCPSRWrite(VariableArg src, IROp *op) {
    RecordPSRWrite(0, src, op);
}

void DeadRegisterStoreEliminationOptimizerPass::RecordSPSRRead(arm::Mode mode, VariableArg var, IROp *loadOp) {
    RecordPSRRead(SPSRIndex(mode), var, loadOp);
}

void DeadRegisterStoreEliminationOptimizerPass::RecordSPSRWrite(arm::Mode mode, VariableArg src, IROp *op) {
    RecordPSRWrite(SPSRIndex(mode), src, op);
}

void DeadRegisterStoreEliminationOptimizerPass::RecordPSRRead(size_t index, VariableArg var, IROp *loadOp) {
    m_psrWrites[index] = nullptr; // Leave previous write instruction alone

    if (!var.var.IsPresent()) {
        return;
    }

    // Assign variable to current PSR version
    auto &psrVersion = m_psrVersions[index];
    const auto versionIndex = psrVersion - 1; // Versions are 1-indexed
    ResizeVersionToVarMap(versionIndex);
    auto &versionEntry = m_versionToVarMap[versionIndex];
    if (!versionEntry.var.IsPresent()) {
        versionEntry.var = var.var;
    }

    // Assign PSR version to the variable
    const auto varIndex = var.var.Index();
    ResizeVarToVersionMap(varIndex);
    m_varToVersionMap[varIndex] = psrVersion;

    // If the current version of the PSR comes from a previous store without modifications, erase both instructions
    if (versionEntry.writeOp != nullptr) {
        m_emitter.Erase(loadOp);
        m_emitter.Erase(versionEntry.writeOp);
        versionEntry.writeOp = nullptr;
    }
}

void DeadRegisterStoreEliminationOptimizerPass::RecordPSRWrite(size_t index, VariableArg src, IROp *op) {
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
    if (varIndex < m_varToVersionMap.size() && m_varToVersionMap[varIndex] != 0) {
        auto version = m_varToVersionMap[varIndex];
        if (version == psrVersion) {
            // No changes were made; erase this write
            m_emitter.Erase(op);
        }
        psrVersion = version;

        // Associate this version with the given write op
        const auto index = psrVersion - 1;        // Versions are 1-indexed
        assert(index < m_versionToVarMap.size()); // this entry should exist
        m_versionToVarMap[index].writeOp = op;
    } else {
        // Set PSR to the next version
        psrVersion = m_nextVersion;
    }
    ++m_nextVersion;
}

bool DeadRegisterStoreEliminationOptimizerPass::IsTagged(VariableArg var) {
    if (!var.var.IsPresent()) {
        return false;
    }

    const auto varIndex = var.var.Index();
    if (varIndex < m_varToVersionMap.size()) {
        return m_varToVersionMap[varIndex] != 0;
    }
    return false;
}

bool DeadRegisterStoreEliminationOptimizerPass::IsTagged(VarOrImmArg var) {
    if (var.immediate) {
        return false;
    }
    return IsTagged(var.var);
}

void DeadRegisterStoreEliminationOptimizerPass::AssignNewVersion(VariableArg var) {
    if (!var.var.IsPresent()) {
        return;
    }

    const auto varIndex = var.var.Index();
    ResizeVarToVersionMap(varIndex);
    m_varToVersionMap[varIndex] = m_nextVersion++;

    const auto versionIndex = m_varToVersionMap[varIndex] - 1;
    ResizeVersionToVarMap(versionIndex);
    m_versionToVarMap[versionIndex].var = var.var;
}

void DeadRegisterStoreEliminationOptimizerPass::CopyVersion(VariableArg dst, VariableArg src) {
    if (!dst.var.IsPresent() || !src.var.IsPresent()) {
        return;
    }

    const auto srcIndex = src.var.Index();
    if (srcIndex >= m_varToVersionMap.size()) {
        return;
    }
    if (m_varToVersionMap[srcIndex] == 0) {
        return;
    }

    const auto dstIndex = dst.var.Index();
    ResizeVarToVersionMap(dstIndex);
    m_varToVersionMap[dstIndex] = m_varToVersionMap[srcIndex];

    const auto versionIndex = m_varToVersionMap[dstIndex] - 1;
    ResizeVersionToVarMap(versionIndex);
    m_versionToVarMap[versionIndex].var = dst.var;
}

void DeadRegisterStoreEliminationOptimizerPass::SubstituteVar(VariableArg &var) {
    if (!var.var.IsPresent()) {
        return;
    }

    // Check if the variable is tagged with a version
    const auto varIndex = var.var.Index();
    if (varIndex >= m_varToVersionMap.size()) {
        return;
    }
    const auto version = m_varToVersionMap[varIndex];
    if (version == 0) {
        return;
    }

    // Replace variable with the one corresponding to this version, if present
    const auto versionIndex = version - 1;
    if (versionIndex >= m_versionToVarMap.size()) {
        return;
    }
    auto &entry = m_versionToVarMap[versionIndex];
    if (entry.var.IsPresent()) {
        MarkDirty(var != entry.var);
        var = entry.var;
    }
}

void DeadRegisterStoreEliminationOptimizerPass::SubstituteVar(VarOrImmArg &var) {
    if (var.immediate) {
        return;
    }
    SubstituteVar(var.var);
}

void DeadRegisterStoreEliminationOptimizerPass::ResizeVersionToVarMap(size_t index) {
    if (m_versionToVarMap.size() <= index) {
        m_versionToVarMap.resize(index + 1);
    }
}

void DeadRegisterStoreEliminationOptimizerPass::ResizeVarToVersionMap(size_t index) {
    if (m_varToVersionMap.size() <= index) {
        m_varToVersionMap.resize(index + 1);
    }
}

} // namespace armajitto::ir
