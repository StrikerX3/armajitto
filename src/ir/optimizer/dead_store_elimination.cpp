#include "dead_store_elimination.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

#include <cstdio>
#include <format>

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
    UndefineCPSRBits(op->dst, ~0);
}

void DeadStoreEliminationOptimizerPass::Process(IRSetRegisterOp *op) {
    RecordRead(op->src);
    RecordWrite(op->dst, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRGetCPSROp *op) {
    RecordCPSRRead();
    RecordWrite(op->dst, op);
    InitCPSRBits(op->dst);
}

void DeadStoreEliminationOptimizerPass::Process(IRSetCPSROp *op) {
    RecordRead(op->src);
    RecordCPSRWrite(op);

    // TODO: revisit this

    auto opStr = op->ToString();
    printf("now checking:  %s\n", opStr.c_str());
    if (op->src.immediate) {
        // Immediate value makes every bit known
        m_knownCPSRBits.mask = ~0;
        m_knownCPSRBits.values = op->src.imm.value;
        printf("  immediate!\n");
        UpdateCPSRBitWrites(op, ~0);
    } else if (!op->src.immediate && op->src.var.var.IsPresent()) {
        // Check for derived values
        const auto index = op->src.var.var.Index();
        if (index < m_cpsrBitsPerVar.size()) {
            auto &bits = m_cpsrBitsPerVar[index];
            if (bits.valid) {
                // Check for differences between current CPSR value and the one coming from the variable
                const uint32_t maskDelta = (bits.knownBits.mask ^ m_knownCPSRBits.mask) | bits.undefinedBits;
                const uint32_t valsDelta = (bits.knownBits.values ^ m_knownCPSRBits.values) | bits.undefinedBits;
                printf("  found valid entry! delta: 0x%08x 0x%08x\n", maskDelta, valsDelta);
                if (m_knownCPSRBits.mask != 0 && maskDelta == 0 && valsDelta == 0) {
                    // All masked bits are equal; CPSR value has not changed
                    printf("    no changes! erasing instruction\n");
                    m_emitter.Erase(op);
                } else {
                    // Either the mask or the value (or both) changed
                    printf("    applying changes -> 0x%08x 0x%08x\n", bits.changedBits.mask, bits.changedBits.values);
                    m_knownCPSRBits = bits.knownBits;
                    UpdateCPSRBitWrites(op, bits.changedBits.mask | bits.undefinedBits);
                }
            }
        }
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRGetSPSROp *op) {
    RecordSPSRRead(op->mode);
    RecordWrite(op->dst, op);
    UndefineCPSRBits(op->dst, ~0);
}

void DeadStoreEliminationOptimizerPass::Process(IRSetSPSROp *op) {
    RecordRead(op->src);
    RecordSPSRWrite(op->mode, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRMemReadOp *op) {
    RecordRead(op->address, true);
    RecordDependentRead(op->dst, op->address);
    RecordWrite(op->dst, op);
    UndefineCPSRBits(op->dst, ~0);
}

void DeadStoreEliminationOptimizerPass::Process(IRMemWriteOp *op) {
    RecordRead(op->src);
    RecordRead(op->address);
}

void DeadStoreEliminationOptimizerPass::Process(IRPreloadOp *op) {
    RecordRead(op->address);
}

void DeadStoreEliminationOptimizerPass::Process(IRLogicalShiftLeftOp *op) {
    RecordRead(op->value, true);
    RecordRead(op->amount, true);
    RecordDependentRead(op->dst, op->value);
    RecordDependentRead(op->dst, op->amount);
    if (op->setCarry) {
        RecordHostFlagsWrite(arm::Flags::C, op);
    }
    RecordWrite(op->dst, op);
    UndefineCPSRBits(op->dst, ~0);
}

void DeadStoreEliminationOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    RecordRead(op->value, true);
    RecordRead(op->amount, true);
    RecordDependentRead(op->dst, op->value);
    RecordDependentRead(op->dst, op->amount);
    if (op->setCarry) {
        RecordHostFlagsWrite(arm::Flags::C, op);
    }
    RecordWrite(op->dst, op);
    UndefineCPSRBits(op->dst, ~0);
}

void DeadStoreEliminationOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    RecordRead(op->value, true);
    RecordRead(op->amount, true);
    RecordDependentRead(op->dst, op->value);
    RecordDependentRead(op->dst, op->amount);
    if (op->setCarry) {
        RecordHostFlagsWrite(arm::Flags::C, op);
    }
    RecordWrite(op->dst, op);
    UndefineCPSRBits(op->dst, ~0);
}

void DeadStoreEliminationOptimizerPass::Process(IRRotateRightOp *op) {
    RecordRead(op->value, true);
    RecordRead(op->amount, true);
    RecordDependentRead(op->dst, op->value);
    RecordDependentRead(op->dst, op->amount);
    if (op->setCarry) {
        RecordHostFlagsWrite(arm::Flags::C, op);
    }
    RecordWrite(op->dst, op);
    UndefineCPSRBits(op->dst, ~0);
}

void DeadStoreEliminationOptimizerPass::Process(IRRotateRightExtendOp *op) {
    RecordHostFlagsRead(arm::Flags::C);
    RecordRead(op->value, true);
    RecordDependentRead(op->dst, op->value);
    RecordWrite(op->dst, op);
    if (op->setCarry) {
        RecordHostFlagsWrite(arm::Flags::C, op);
    }
    UndefineCPSRBits(op->dst, ~0);
}

void DeadStoreEliminationOptimizerPass::Process(IRBitwiseAndOp *op) {
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);

    // AND clears all zero bits
    if (op->lhs.immediate) {
        DeriveCPSRBits(op->dst, op->rhs.var, ~op->lhs.imm.value, 0);
    } else if (op->rhs.immediate) {
        DeriveCPSRBits(op->dst, op->lhs.var, ~op->rhs.imm.value, 0);
    } else {
        UndefineCPSRBits(op->dst, ~0);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRBitwiseOrOp *op) {
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);

    // OR sets all one bits
    if (op->lhs.immediate) {
        DeriveCPSRBits(op->dst, op->rhs.var, op->lhs.imm.value, op->lhs.imm.value);
    } else if (op->rhs.immediate) {
        DeriveCPSRBits(op->dst, op->lhs.var, op->rhs.imm.value, op->rhs.imm.value);
    } else {
        UndefineCPSRBits(op->dst, ~0);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRBitwiseXorOp *op) {
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);

    // XOR flips all one bits
    if (op->lhs.immediate) {
        UndefineCPSRBits(op->dst, op->lhs.imm.value);
    } else if (op->rhs.immediate) {
        UndefineCPSRBits(op->dst, op->rhs.imm.value);
    } else {
        UndefineCPSRBits(op->dst, ~0);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRBitClearOp *op) {
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);

    // BIC clears all one bits
    if (op->lhs.immediate) {
        DeriveCPSRBits(op->dst, op->rhs.var, op->lhs.imm.value, 0);
    } else if (op->rhs.immediate) {
        DeriveCPSRBits(op->dst, op->lhs.var, op->rhs.imm.value, 0);
    } else {
        UndefineCPSRBits(op->dst, ~0);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRCountLeadingZerosOp *op) {
    RecordRead(op->value, true);
    RecordDependentRead(op->dst, op->value);
    RecordWrite(op->dst, op);
    UndefineCPSRBits(op->dst, ~0);
}

void DeadStoreEliminationOptimizerPass::Process(IRAddOp *op) {
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);
    UndefineCPSRBits(op->dst, ~0);
}

void DeadStoreEliminationOptimizerPass::Process(IRAddCarryOp *op) {
    RecordHostFlagsRead(arm::Flags::C);
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);
    UndefineCPSRBits(op->dst, ~0);
}

void DeadStoreEliminationOptimizerPass::Process(IRSubtractOp *op) {
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);
    UndefineCPSRBits(op->dst, ~0);
}

void DeadStoreEliminationOptimizerPass::Process(IRSubtractCarryOp *op) {
    RecordHostFlagsRead(arm::Flags::C);
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);
    UndefineCPSRBits(op->dst, ~0);
}

void DeadStoreEliminationOptimizerPass::Process(IRMoveOp *op) {
    if (op->flags != arm::Flags::None) {
        RecordRead(op->value, true);
        RecordDependentRead(op->dst, op->value);
        RecordHostFlagsWrite(op->flags, op);
    } else {
        RecordRead(op->value, false);
        RecordDependentRead(op->dst, op->value);
    }
    RecordWrite(op->dst, op);
    if (op->value.immediate) {
        DefineCPSRBits(op->dst, ~0, op->value.imm.value);
    } else {
        CopyCPSRBits(op->dst, op->value.var);
    }
}

void DeadStoreEliminationOptimizerPass::Process(IRMoveNegatedOp *op) {
    RecordRead(op->value, true);
    RecordDependentRead(op->dst, op->value);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);
    UndefineCPSRBits(op->dst, ~0);
}

void DeadStoreEliminationOptimizerPass::Process(IRSaturatingAddOp *op) {
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);
    UndefineCPSRBits(op->dst, ~0);
}

void DeadStoreEliminationOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);
    UndefineCPSRBits(op->dst, ~0);
}

void DeadStoreEliminationOptimizerPass::Process(IRMultiplyOp *op) {
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dst, op->lhs);
    RecordDependentRead(op->dst, op->rhs);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dst, op);
    UndefineCPSRBits(op->dst, ~0);
}

void DeadStoreEliminationOptimizerPass::Process(IRMultiplyLongOp *op) {
    RecordRead(op->lhs, true);
    RecordRead(op->rhs, true);
    RecordDependentRead(op->dstLo, op->lhs);
    RecordDependentRead(op->dstLo, op->rhs);
    RecordDependentRead(op->dstHi, op->lhs);
    RecordDependentRead(op->dstHi, op->rhs);
    RecordHostFlagsWrite(op->flags, op);
    RecordWrite(op->dstLo, op);
    RecordWrite(op->dstHi, op);
    UndefineCPSRBits(op->dstLo, ~0);
    UndefineCPSRBits(op->dstHi, ~0);
}

void DeadStoreEliminationOptimizerPass::Process(IRAddLongOp *op) {
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
    UndefineCPSRBits(op->dstLo, ~0);
    UndefineCPSRBits(op->dstHi, ~0);
}

void DeadStoreEliminationOptimizerPass::Process(IRStoreFlagsOp *op) {
    RecordHostFlagsWrite(op->flags, op);
}

void DeadStoreEliminationOptimizerPass::Process(IRLoadFlagsOp *op) {
    RecordHostFlagsRead(op->flags);
    RecordRead(op->srcCPSR, true);
    RecordDependentRead(op->dstCPSR, op->srcCPSR);
    RecordWrite(op->dstCPSR, op);
    UndefineCPSRBits(op->dstCPSR, static_cast<uint32_t>(op->flags));
}

void DeadStoreEliminationOptimizerPass::Process(IRLoadStickyOverflowOp *op) {
    RecordHostFlagsRead(arm::Flags::Q);
    RecordRead(op->srcCPSR, true);
    RecordDependentRead(op->dstCPSR, op->srcCPSR);
    RecordWrite(op->dstCPSR, op);
    UndefineCPSRBits(op->dstCPSR, static_cast<uint32_t>(arm::Flags::Q));
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
    UndefineCPSRBits(op->dstValue, ~0);
}

void DeadStoreEliminationOptimizerPass::Process(IRStoreCopRegisterOp *op) {
    RecordRead(op->srcValue);
}

void DeadStoreEliminationOptimizerPass::Process(IRConstantOp *op) {
    RecordWrite(op->dst, op);
    DefineCPSRBits(op->dst, ~0, op->value);
}

void DeadStoreEliminationOptimizerPass::Process(IRCopyVarOp *op) {
    RecordRead(op->var, false);
    RecordDependentRead(op->dst, op->var);
    RecordWrite(op->dst, op);
    CopyCPSRBits(op->dst, op->var);
}

void DeadStoreEliminationOptimizerPass::Process(IRGetBaseVectorAddressOp *op) {
    RecordWrite(op->dst, op);
    UndefineCPSRBits(op->dst, ~0);
}

// ---------------------------------------------------------------------------------------------------------------------
// Variable read, write and consumption tracking

void DeadStoreEliminationOptimizerPass::RecordRead(VariableArg dst, bool consume) {
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

void DeadStoreEliminationOptimizerPass::RecordRead(VarOrImmArg dst, bool consume) {
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
// GPR and PSR read and write tracking

void DeadStoreEliminationOptimizerPass::RecordRead(GPRArg gpr) {
    m_gprWrites[gpr.Index()] = nullptr; // Leave instruction alone
}

void DeadStoreEliminationOptimizerPass::RecordWrite(GPRArg gpr, IROp *op) {
    auto gprIndex = gpr.Index();
    IROp *writeOp = m_gprWrites[gprIndex];
    if (writeOp != nullptr) {
        // GPR is overwritten
        // Erase previous instruction, which is always going to be an IRSetRegisterOp
        m_emitter.Erase(writeOp);
    }
    m_gprWrites[gprIndex] = op;
}

void DeadStoreEliminationOptimizerPass::RecordCPSRRead() {
    m_cpsrWrite = nullptr; // Leave instruction alone
}

void DeadStoreEliminationOptimizerPass::RecordCPSRWrite(IROp *op) {
    if (m_cpsrWrite != nullptr) {
        // CPSR is overwritten
        // Erase previous instruction, which is always going to be an IRSetCPSROp
        m_emitter.Erase(m_cpsrWrite);
    }
    m_cpsrWrite = op;
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
                VisitIROp(write, [this, flag](auto op) -> void { EraseWrite(flag, op); });
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
// CPSR bits tracking

void DeadStoreEliminationOptimizerPass::ResizeCPSRBitsPerVar(size_t size) {
    if (m_cpsrBitsPerVar.size() <= size) {
        m_cpsrBitsPerVar.resize(size + 1);
    }
}

void DeadStoreEliminationOptimizerPass::InitCPSRBits(VariableArg dst) {
    if (!dst.var.IsPresent()) {
        return;
    }
    const auto index = dst.var.Index();
    ResizeCPSRBitsPerVar(index);
    CPSRBits &bits = m_cpsrBitsPerVar[index];
    bits.valid = true;
    bits.knownBits = m_knownCPSRBits;

    auto dstStr = dst.ToString();
    printf("%s = [cpsr] bits=0x%08x vals=0x%08x\n", dstStr.c_str(), m_knownCPSRBits.mask, m_knownCPSRBits.values);
}

void DeadStoreEliminationOptimizerPass::DeriveCPSRBits(VariableArg dst, VariableArg src, uint32_t mask,
                                                       uint32_t value) {
    if (!dst.var.IsPresent() || !src.var.IsPresent()) {
        return;
    }
    const auto dstIndex = dst.var.Index();
    const auto srcIndex = src.var.Index();
    ResizeCPSRBitsPerVar(dstIndex);
    if (srcIndex >= m_cpsrBitsPerVar.size()) {
        // This shouldn't happen
        return;
    }
    auto &srcBits = m_cpsrBitsPerVar[srcIndex];
    if (!srcBits.valid) {
        // Not a CPSR value; don't care
        return;
    }
    auto &dstBits = m_cpsrBitsPerVar[dstIndex];
    dstBits.valid = true;
    dstBits.knownBits.mask = srcBits.knownBits.mask | mask;
    dstBits.knownBits.values = (srcBits.knownBits.values & ~mask) | (value & mask);
    dstBits.changedBits.mask = srcBits.changedBits.mask | mask;
    dstBits.changedBits.values = (srcBits.changedBits.values & ~mask) | (value & mask);
    dstBits.undefinedBits = srcBits.undefinedBits;

    auto dstStr = dst.ToString();
    auto srcStr = src.ToString();
    printf("%s = [derived] src=%s bits=0x%08x vals=0x%08x newbits=0x%08x newvals=0x%08x undefs=0x%08x\n",
           dstStr.c_str(), srcStr.c_str(), dstBits.knownBits.mask, dstBits.knownBits.values, dstBits.changedBits.mask,
           dstBits.changedBits.values, dstBits.undefinedBits);
}

void DeadStoreEliminationOptimizerPass::CopyCPSRBits(VariableArg dst, VariableArg src) {
    if (!dst.var.IsPresent() || !src.var.IsPresent()) {
        return;
    }
    const auto dstIndex = dst.var.Index();
    const auto srcIndex = src.var.Index();
    ResizeCPSRBitsPerVar(dstIndex);
    if (srcIndex >= m_cpsrBitsPerVar.size()) {
        // This shouldn't happen
        return;
    }
    auto &srcBits = m_cpsrBitsPerVar[srcIndex];
    if (!srcBits.valid) {
        // Not a CPSR value; don't care
        return;
    }
    auto &dstBits = m_cpsrBitsPerVar[dstIndex];
    dstBits = srcBits;

    auto dstStr = dst.ToString();
    auto srcStr = src.ToString();
    printf("%s = [copied] src=%s bits=0x%08x vals=0x%08x newbits=0x%08x newvals=0x%08x undefs=0x%08x\n", dstStr.c_str(),
           srcStr.c_str(), dstBits.knownBits.mask, dstBits.knownBits.values, dstBits.changedBits.mask,
           dstBits.changedBits.values, dstBits.undefinedBits);
}

void DeadStoreEliminationOptimizerPass::DefineCPSRBits(VariableArg dst, uint32_t mask, uint32_t value) {
    if (!dst.var.IsPresent()) {
        return;
    }
    const auto dstIndex = dst.var.Index();
    ResizeCPSRBitsPerVar(dstIndex);
    auto &dstBits = m_cpsrBitsPerVar[dstIndex];
    dstBits.valid = true;
    dstBits.knownBits.mask = mask;
    dstBits.knownBits.values = value & mask;
    dstBits.changedBits.mask = mask;
    dstBits.changedBits.values = value & mask;

    auto dstStr = dst.ToString();
    printf("%s = [defined] bits=0x%08x vals=0x%08x newbits=0x%08x newvals=0x%08x undefs=0x%08x\n", dstStr.c_str(),
           dstBits.knownBits.mask, dstBits.knownBits.values, dstBits.changedBits.mask, dstBits.changedBits.values,
           dstBits.undefinedBits);
}

void DeadStoreEliminationOptimizerPass::UndefineCPSRBits(VariableArg dst, uint32_t mask) {
    if (!dst.var.IsPresent()) {
        return;
    }
    const auto dstIndex = dst.var.Index();
    ResizeCPSRBitsPerVar(dstIndex);
    auto &dstBits = m_cpsrBitsPerVar[dstIndex];
    dstBits.valid = true;
    dstBits.undefinedBits = mask;

    auto dstStr = dst.ToString();
    printf("%s = [undefined] bits=0x%08x\n", dstStr.c_str(), mask);
}

void DeadStoreEliminationOptimizerPass::UpdateCPSRBitWrites(IROp *op, uint32_t mask) {
    for (uint32_t i = 0; i < 32; i++) {
        const uint32_t bit = (1 << i);
        if (!(mask & bit)) {
            continue;
        }

        // Check for previous write
        auto *prevOp = m_cpsrBitWrites[i];
        if (prevOp != nullptr) {
            // Clear bit from mask
            auto &writeMask = m_cpsrBitWriteMasks[prevOp];
            writeMask &= ~bit;
            if (writeMask == 0) {
                // Instruction no longer writes anything useful; erase it
                auto str = prevOp->ToString();
                printf("    erasing %s\n", str.c_str());
                m_emitter.Erase(prevOp);
                m_cpsrBitWriteMasks.erase(prevOp);
            }
        }

        // Update reference to this instruction and update mask
        m_cpsrBitWrites[i] = op;
        m_cpsrBitWriteMasks[op] |= bit;
    }
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
                if (!write.consumed) {
                    EraseWriteRecursive(dep, write.op);
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

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRLoadFlagsOp *op) {
    if (op->dstCPSR == var) {
        op->dstCPSR.var = {};
    }
    return EraseInstruction(op);
}

bool DeadStoreEliminationOptimizerPass::EraseWrite(Variable var, IRLoadStickyOverflowOp *op) {
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
// Generic EraseWrite for flags

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
        m_emitter.Erase(op);
    }
}

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRLoadFlagsOp *op) {
    op->flags &= ~flag;
    if (op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
    }
}

void DeadStoreEliminationOptimizerPass::EraseWrite(arm::Flags flag, IRLoadStickyOverflowOp *op) {
    if (flag == arm::Flags::Q) {
        m_emitter.Erase(op);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Generic EraseInstruction

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
    if (!op->dst.var.IsPresent() && !op->setCarry) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRLogicalShiftRightOp *op) {
    if (!op->dst.var.IsPresent() && !op->setCarry) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRArithmeticShiftRightOp *op) {
    if (!op->dst.var.IsPresent() && !op->setCarry) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRRotateRightOp *op) {
    if (!op->dst.var.IsPresent() && !op->setCarry) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRRotateRightExtendOp *op) {
    if (!op->dst.var.IsPresent() && !op->setCarry) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRBitwiseAndOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRBitwiseOrOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRBitwiseXorOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRBitClearOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
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
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRAddCarryOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRSubtractOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRSubtractCarryOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRMoveOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRMoveNegatedOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRSaturatingAddOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRSaturatingSubtractOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRMultiplyOp *op) {
    if (!op->dst.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRMultiplyLongOp *op) {
    if (!op->dstLo.var.IsPresent() && !op->dstHi.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRAddLongOp *op) {
    if (!op->dstLo.var.IsPresent() && !op->dstHi.var.IsPresent() && op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRStoreFlagsOp *op) {
    if (op->flags == arm::Flags::None) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRLoadFlagsOp *op) {
    if (!op->dstCPSR.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
    }
    return false;
}

bool DeadStoreEliminationOptimizerPass::EraseInstruction(IRLoadStickyOverflowOp *op) {
    if (!op->dstCPSR.var.IsPresent()) {
        m_emitter.Erase(op);
        return true;
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
