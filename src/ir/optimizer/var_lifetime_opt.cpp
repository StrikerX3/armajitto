#include "var_lifetime_opt.hpp"

namespace armajitto::ir {

VarLifetimeOptimizerPass::VarLifetimeOptimizerPass(Emitter &emitter)
    : OptimizerPassBase(emitter) {}

void VarLifetimeOptimizerPass::PostProcess() {}

void VarLifetimeOptimizerPass::Process(IRGetRegisterOp *op) {
    RecordRead(op->src);
    RecordWrite(op->dst);
}

void VarLifetimeOptimizerPass::Process(IRSetRegisterOp *op) {
    RecordRead(op->src);
    RecordWrite(op->dst);
}

void VarLifetimeOptimizerPass::Process(IRGetCPSROp *op) {
    RecordCPSRRead();
    RecordWrite(op->dst);
}

void VarLifetimeOptimizerPass::Process(IRSetCPSROp *op) {
    RecordRead(op->src);
    RecordCPSRWrite();
}

void VarLifetimeOptimizerPass::Process(IRGetSPSROp *op) {
    RecordSPSRRead(op->mode);
    RecordWrite(op->dst);
}

void VarLifetimeOptimizerPass::Process(IRSetSPSROp *op) {
    RecordRead(op->src);
    RecordSPSRWrite(op->mode);
}

void VarLifetimeOptimizerPass::Process(IRMemReadOp *op) {
    RecordRead(op->address);
    RecordWrite(op->dst);
}

void VarLifetimeOptimizerPass::Process(IRMemWriteOp *op) {
    RecordRead(op->address);
    RecordRead(op->src);
}

void VarLifetimeOptimizerPass::Process(IRPreloadOp *op) {
    RecordRead(op->address);
}

void VarLifetimeOptimizerPass::Process(IRLogicalShiftLeftOp *op) {
    RecordRead(op->value);
    RecordRead(op->amount);
    RecordWrite(op->dst);
    if (op->setCarry) {
        RecordWrite(arm::Flags::C);
    }
}

void VarLifetimeOptimizerPass::Process(IRLogicalShiftRightOp *op) {
    RecordRead(op->value);
    RecordRead(op->amount);
    RecordWrite(op->dst);
    if (op->setCarry) {
        RecordWrite(arm::Flags::C);
    }
}

void VarLifetimeOptimizerPass::Process(IRArithmeticShiftRightOp *op) {
    RecordRead(op->value);
    RecordRead(op->amount);
    RecordWrite(op->dst);
    if (op->setCarry) {
        RecordWrite(arm::Flags::C);
    }
}

void VarLifetimeOptimizerPass::Process(IRRotateRightOp *op) {
    RecordRead(op->value);
    RecordRead(op->amount);
    RecordWrite(op->dst);
    if (op->setCarry) {
        RecordWrite(arm::Flags::C);
    }
}

void VarLifetimeOptimizerPass::Process(IRRotateRightExtendedOp *op) {
    RecordRead(op->value);
    RecordRead(arm::Flags::C);
    RecordWrite(op->dst);
    if (op->setCarry) {
        RecordWrite(arm::Flags::C);
    }
}

void VarLifetimeOptimizerPass::Process(IRBitwiseAndOp *op) {
    RecordRead(op->lhs);
    RecordRead(op->rhs);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRBitwiseOrOp *op) {
    RecordRead(op->lhs);
    RecordRead(op->rhs);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRBitwiseXorOp *op) {
    RecordRead(op->lhs);
    RecordRead(op->rhs);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRBitClearOp *op) {
    RecordRead(op->lhs);
    RecordRead(op->rhs);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRCountLeadingZerosOp *op) {
    RecordRead(op->value);
    RecordWrite(op->dst);
}

void VarLifetimeOptimizerPass::Process(IRAddOp *op) {
    RecordRead(op->lhs);
    RecordRead(op->rhs);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRAddCarryOp *op) {
    RecordRead(op->lhs);
    RecordRead(op->rhs);
    RecordRead(arm::Flags::C);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRSubtractOp *op) {
    RecordRead(op->lhs);
    RecordRead(op->rhs);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRSubtractCarryOp *op) {
    RecordRead(op->lhs);
    RecordRead(op->rhs);
    RecordRead(arm::Flags::C);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRMoveOp *op) {
    RecordRead(op->value);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRMoveNegatedOp *op) {
    RecordRead(op->value);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRSaturatingAddOp *op) {
    RecordRead(op->lhs);
    RecordRead(op->rhs);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRSaturatingSubtractOp *op) {
    RecordRead(op->lhs);
    RecordRead(op->rhs);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRMultiplyOp *op) {
    RecordRead(op->lhs);
    RecordRead(op->rhs);
    RecordWrite(op->dst);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRMultiplyLongOp *op) {
    RecordRead(op->lhs);
    RecordRead(op->rhs);
    RecordWrite(op->dstLo);
    RecordWrite(op->dstHi);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRAddLongOp *op) {
    RecordRead(op->lhsLo);
    RecordRead(op->lhsHi);
    RecordRead(op->rhsLo);
    RecordRead(op->rhsHi);
    RecordWrite(op->dstLo);
    RecordWrite(op->dstHi);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRStoreFlagsOp *op) {
    RecordRead(op->values);
    RecordWrite(op->flags);
}

void VarLifetimeOptimizerPass::Process(IRLoadFlagsOp *op) {
    RecordRead(op->srcCPSR);
    RecordRead(op->flags);
    RecordWrite(op->dstCPSR);
}

void VarLifetimeOptimizerPass::Process(IRLoadStickyOverflowOp *op) {
    RecordRead(op->srcCPSR);
    if (op->setQ) {
        RecordRead(arm::Flags::V);
    }
    RecordWrite(op->dstCPSR);
}

void VarLifetimeOptimizerPass::Process(IRBranchOp *op) {
    RecordRead(op->address);
    RecordCPSRRead();
    RecordWrite(arm::GPR::PC);
}

void VarLifetimeOptimizerPass::Process(IRBranchExchangeOp *op) {
    RecordRead(op->address);
    RecordCPSRRead();
    RecordWrite(arm::GPR::PC);
    RecordCPSRWrite();
}

void VarLifetimeOptimizerPass::Process(IRLoadCopRegisterOp *op) {
    RecordWrite(op->dstValue);
}

void VarLifetimeOptimizerPass::Process(IRStoreCopRegisterOp *op) {
    RecordRead(op->srcValue);
}

void VarLifetimeOptimizerPass::Process(IRConstantOp *op) {
    RecordWrite(op->dst);
}

void VarLifetimeOptimizerPass::Process(IRCopyVarOp *op) {
    RecordRead(op->var);
    RecordWrite(op->dst);
}

void VarLifetimeOptimizerPass::Process(IRGetBaseVectorAddressOp *op) {
    RecordWrite(op->dst);
}

// ---------------------------------------------------------------------------------------------------------------------
// Read/write tracking

static inline size_t SPSRIndex(arm::Mode mode) {
    return arm::NormalizedIndex(mode) + 1;
}

void VarLifetimeOptimizerPass::RecordRead(VarOrImmArg arg) {
    if (!arg.immediate) {
        RecordRead(arg.var);
    }
}

void VarLifetimeOptimizerPass::RecordRead(VariableArg arg) {
    // TODO: implement
}

void VarLifetimeOptimizerPass::RecordRead(GPRArg arg) {
    // TODO: implement
}

void VarLifetimeOptimizerPass::RecordCPSRRead() {
    RecordPSRRead(0);
}

void VarLifetimeOptimizerPass::RecordSPSRRead(arm::Mode mode) {
    RecordPSRRead(SPSRIndex(mode));
}

void VarLifetimeOptimizerPass::RecordPSRRead(size_t index) {
    // TODO: implement
}

void VarLifetimeOptimizerPass::RecordRead(arm::Flags flags) {
    // TODO: implement
}

void VarLifetimeOptimizerPass::RecordWrite(VarOrImmArg arg) {
    if (!arg.immediate) {
        RecordWrite(arg.var);
    }
}

void VarLifetimeOptimizerPass::RecordWrite(VariableArg arg) {
    // TODO: implement
}

void VarLifetimeOptimizerPass::RecordWrite(GPRArg arg) {
    // TODO: implement
}

void VarLifetimeOptimizerPass::RecordCPSRWrite() {
    RecordPSRWrite(0);
}

void VarLifetimeOptimizerPass::RecordSPSRWrite(arm::Mode mode) {
    RecordPSRWrite(SPSRIndex(mode));
}

void VarLifetimeOptimizerPass::RecordPSRWrite(size_t index) {
    // TODO: implement
}

void VarLifetimeOptimizerPass::RecordWrite(arm::Flags flags) {
    // TODO: implement
}

} // namespace armajitto::ir
