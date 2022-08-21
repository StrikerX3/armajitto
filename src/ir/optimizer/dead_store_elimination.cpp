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
    RecordWrite(op->dst, op);
    RecordRead(op->src);
}

void DeadStoreEliminationOptimizerPass::Process(IRSetRegisterOp *op) {
    RecordWrite(op->dst, op);
    RecordRead(op->src);
}

void DeadStoreEliminationOptimizerPass::Process(IRGetCPSROp *op) {
    RecordWrite(op->dst, op);
    // TODO: deal with CPSR and flags
}

void DeadStoreEliminationOptimizerPass::Process(IRSetCPSROp *op) {
    RecordRead(op->src);
    // TODO: deal with CPSR and flags
}

void DeadStoreEliminationOptimizerPass::Process(IRGetSPSROp *op) {
    RecordWrite(op->dst, op);
    // TODO: deal with SPSRs
}

void DeadStoreEliminationOptimizerPass::Process(IRSetSPSROp *op) {
    RecordRead(op->src);
    // TODO: deal with SPSRs
}

void DeadStoreEliminationOptimizerPass::Process(IRMemReadOp *op) {
    RecordWrite(op->dst, op);
    RecordDependentRead(op->dst, op->address);
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
    RecordDependentRead(op->dst, op->value);
    RecordDependentRead(op->dst, op->amount);
    if (op->setFlags) {
        // TODO: C flag is written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    RecordWrite(op->dst, op);
    RecordDependentRead(op->dst, op->value);
    RecordDependentRead(op->dst, op->amount);
    if (op->setFlags) {
        // TODO: C flag is written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    RecordWrite(op->dst, op);
    RecordDependentRead(op->dst, op->value);
    RecordDependentRead(op->dst, op->amount);
    if (op->setFlags) {
        // TODO: C flag is written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRRotateRightOp *op) {
    RecordWrite(op->dst, op);
    RecordDependentRead(op->dst, op->value);
    RecordDependentRead(op->dst, op->amount);
    if (op->setFlags) {
        // TODO: C flag is written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRRotateRightExtendOp *op) {
    RecordWrite(op->dst, op);
    RecordDependentRead(op->dst, op->value);
    // TODO: C flag is read
    if (op->setFlags) {
        // TODO: C flag is written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRBitwiseAndOp *op) {
    RecordWrite(op->dst, op);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    if (op->setFlags) {
        // TODO: NZ flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRBitwiseOrOp *op) {
    RecordWrite(op->dst, op);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    if (op->setFlags) {
        // TODO: NZ flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRBitwiseXorOp *op) {
    RecordWrite(op->dst, op);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    if (op->setFlags) {
        // TODO: NZ flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRBitClearOp *op) {
    RecordWrite(op->dst, op);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    if (op->setFlags) {
        // TODO: NZ flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRCountLeadingZerosOp *op) {
    RecordWrite(op->dst, op);
    RecordDependentRead(op->dst, op->value);
}

void DeadStoreEliminationOptimizerPass::Process(IRAddOp *op) {
    RecordWrite(op->dst, op);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    if (op->setFlags) {
        // TODO: NZCV flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRAddCarryOp *op) {
    RecordWrite(op->dst, op);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    // TODO: C flag is read
    if (op->setFlags) {
        // TODO: NZCV flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRSubtractOp *op) {
    RecordWrite(op->dst, op);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    if (op->setFlags) {
        // TODO: NZCV flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRSubtractCarryOp *op) {
    RecordWrite(op->dst, op);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    // TODO: C flag is read
    if (op->setFlags) {
        // TODO: NZCV flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRMoveOp *op) {
    RecordWrite(op->dst, op);
    if (op->setFlags) {
        RecordDependentRead(op->dst, op->value);
        // TODO: NZ flags are written
    } else {
        RecordDependentRead(op->dst, op->value, false);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRMoveNegatedOp *op) {
    RecordWrite(op->dst, op);
    RecordDependentRead(op->dst, op->value);
    if (op->setFlags) {
        // TODO: NZ flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRSaturatingAddOp *op) {
    RecordWrite(op->dst, op);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    // TODO: Q flag is written
}

void DeadStoreEliminationOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    RecordWrite(op->dst, op);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    // TODO: Q flag is written
}

void DeadStoreEliminationOptimizerPass::Process(IRMultiplyOp *op) {
    RecordWrite(op->dst, op);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    if (op->setFlags) {
        // TODO: NZ flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRMultiplyLongOp *op) {
    RecordWrite(op->dstLo, op);
    RecordWrite(op->dstHi, op);
    RecordDependentRead(op->dstLo, op->lhs);
    RecordDependentRead(op->dstLo, op->rhs);
    RecordDependentRead(op->dstHi, op->lhs);
    RecordDependentRead(op->dstHi, op->rhs);
    if (op->setFlags) {
        // TODO: NZ flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRAddLongOp *op) {
    RecordWrite(op->dstLo, op);
    RecordWrite(op->dstHi, op);
    RecordDependentRead(op->dstLo, op->lhsLo);
    RecordDependentRead(op->dstLo, op->lhsHi);
    RecordDependentRead(op->dstLo, op->rhsLo);
    RecordDependentRead(op->dstLo, op->rhsHi);
    RecordDependentRead(op->dstHi, op->lhsLo);
    RecordDependentRead(op->dstHi, op->lhsHi);
    RecordDependentRead(op->dstHi, op->rhsLo);
    RecordDependentRead(op->dstHi, op->rhsHi);
    if (op->setFlags) {
        // TODO: NZ flags are written
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRStoreFlagsOp *op) {
    RecordWrite(op->dstCPSR, op);
    RecordDependentRead(op->dstCPSR, op->srcCPSR);
    RecordDependentRead(op->dstCPSR, op->values);
    // TODO: NZCVQ flags are written depending on op->flags
}

void DeadStoreEliminationOptimizerPass::Process(IRUpdateFlagsOp *op) {
    RecordWrite(op->dstCPSR, op);
    RecordDependentRead(op->dstCPSR, op->srcCPSR);
    // TODO: NZCV flags are written depending on op->flags
}

void DeadStoreEliminationOptimizerPass::Process(IRUpdateStickyOverflowOp *op) {
    RecordWrite(op->dstCPSR, op);
    RecordDependentRead(op->dstCPSR, op->srcCPSR);
    // TODO: Q flag is written depending on op->flags
}

void DeadStoreEliminationOptimizerPass::Process(IRBranchOp *op) {
    RecordWrite(arm::GPR::PC, op);
    RecordRead(op->address);
    // TODO: CPSR is read
}

void DeadStoreEliminationOptimizerPass::Process(IRBranchExchangeOp *op) {
    RecordWrite(arm::GPR::PC, op);
    RecordRead(op->address);
    // TODO: CPSR is written and read
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
    RecordDependentRead(op->dst, op->var, false);
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
    m_varWrites[varIndex].op = op;
    m_varWrites[varIndex].read = false;
    m_varWrites[varIndex].consumed = false;
}

void DeadStoreEliminationOptimizerPass::RecordWrite(VariableArg dst, IROp *op) {
    RecordWrite(dst.var, op);
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

void DeadStoreEliminationOptimizerPass::RecordRead(GPRArg gpr) {
    auto gprIndex = MakeGPRIndex(gpr);
    m_gprWrites[gprIndex] = nullptr; // Leave instruction alone
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
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRGetCPSROp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRGetSPSROp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRMemReadOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent()) {
        if (op->address.immediate /* TODO && no side effects on address */) {
            // TODO: erase only if the memory region is known to have no side-effects (not MMIO, for example)
            // m_emitter.Erase(op);
            // return true;
        }
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRLogicalShiftLeftOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent() && (!op->setFlags || false /* TODO: flags weren't consumed */)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRLogicalShiftRightOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent() && (!op->setFlags || false /* TODO: flags weren't consumed */)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRArithmeticShiftRightOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent() && (!op->setFlags || false /* TODO: flags weren't consumed */)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRRotateRightOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent() && (!op->setFlags || false /* TODO: flags weren't consumed */)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRRotateRightExtendOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent() && (!op->setFlags || false /* TODO: flags weren't consumed */)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRBitwiseAndOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent() && (!op->setFlags || false /* TODO: flags weren't consumed */)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRBitwiseOrOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent() && (!op->setFlags || false /* TODO: flags weren't consumed */)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRBitwiseXorOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent() && (!op->setFlags || false /* TODO: flags weren't consumed */)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRBitClearOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent() && (!op->setFlags || false /* TODO: flags weren't consumed */)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRCountLeadingZerosOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRAddOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent() && (!op->setFlags || false /* TODO: flags weren't consumed */)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRAddCarryOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent() && (!op->setFlags || false /* TODO: flags weren't consumed */)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRSubtractOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent() && (!op->setFlags || false /* TODO: flags weren't consumed */)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRSubtractCarryOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent() && (!op->setFlags || false /* TODO: flags weren't consumed */)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRMoveOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent() && (!op->setFlags || false /* TODO: flags weren't consumed */)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRMoveNegatedOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent() && (!op->setFlags || false /* TODO: flags weren't consumed */)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRSaturatingAddOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent() && false /* TODO: Q flag wasn't consumed */) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRSaturatingSubtractOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent() && false /* TODO: Q flag wasn't consumed */) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRMultiplyOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent() && (!op->setFlags || false /* TODO: flags weren't consumed */)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRMultiplyLongOp *op) {
    if (op->dstLo == var) {
        op->dstLo.var = {};
    }
    if (op->dstHi == var) {
        op->dstHi.var = {};
    }
    if (!op->dstLo.var.IsPresent() && !op->dstHi.var.IsPresent() &&
        (!op->setFlags || false /* TODO: flags weren't consumed */)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRAddLongOp *op) {
    if (op->dstLo == var) {
        op->dstLo.var = {};
    }
    if (op->dstHi == var) {
        op->dstHi.var = {};
    }
    if (!op->dstLo.var.IsPresent() && !op->dstHi.var.IsPresent() &&
        (!op->setFlags || false /* TODO: flags weren't consumed */)) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRStoreFlagsOp *op) {
    if (op->dstCPSR == var) {
        op->dstCPSR.var = {};
    }
    if (!op->dstCPSR.var.IsPresent() && false /* TODO: flags weren't consumed */) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRUpdateFlagsOp *op) {
    if (op->dstCPSR == var) {
        op->dstCPSR.var = {};
    }
    if (!op->dstCPSR.var.IsPresent() && false /* TODO: flags weren't consumed */) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRUpdateStickyOverflowOp *op) {
    if (op->dstCPSR == var) {
        op->dstCPSR.var = {};
    }
    if (!op->dstCPSR.var.IsPresent() && false /* TODO: Q flag wasn't consumed */) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRLoadCopRegisterOp *op) {
    if (op->dstValue == var) {
        op->dstValue.var = {};
    }
    if (!op->dstValue.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRConstantOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRCopyVarOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRGetBaseVectorAddressOp *op) {
    if (op->dst == var) {
        op->dst.var = {};
    }
    if (!op->dst.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

} // namespace armajitto::ir
