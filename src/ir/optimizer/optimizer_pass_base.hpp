#pragma once

#include "armajitto/ir/emitter.hpp"
#include "armajitto/ir/ir_ops.hpp"

namespace armajitto::ir {

// Base class for all optimization passes that implements common algorithms and data structures.
class OptimizerPassBase {
public:
    OptimizerPassBase(Emitter &emitter)
        : m_emitter(emitter) {}

    bool Optimize();

protected:
    Emitter &m_emitter;

    virtual void PreProcess() {}
    virtual void PostProcess() {}

    virtual void Process(IRGetRegisterOp *op) {}
    virtual void Process(IRSetRegisterOp *op) {}
    virtual void Process(IRGetCPSROp *op) {}
    virtual void Process(IRSetCPSROp *op) {}
    virtual void Process(IRGetSPSROp *op) {}
    virtual void Process(IRSetSPSROp *op) {}
    virtual void Process(IRMemReadOp *op) {}
    virtual void Process(IRMemWriteOp *op) {}
    virtual void Process(IRPreloadOp *op) {}
    virtual void Process(IRLogicalShiftLeftOp *op) {}
    virtual void Process(IRLogicalShiftRightOp *op) {}
    virtual void Process(IRArithmeticShiftRightOp *op) {}
    virtual void Process(IRRotateRightOp *op) {}
    virtual void Process(IRRotateRightExtendOp *op) {}
    virtual void Process(IRBitwiseAndOp *op) {}
    virtual void Process(IRBitwiseOrOp *op) {}
    virtual void Process(IRBitwiseXorOp *op) {}
    virtual void Process(IRBitClearOp *op) {}
    virtual void Process(IRCountLeadingZerosOp *op) {}
    virtual void Process(IRAddOp *op) {}
    virtual void Process(IRAddCarryOp *op) {}
    virtual void Process(IRSubtractOp *op) {}
    virtual void Process(IRSubtractCarryOp *op) {}
    virtual void Process(IRMoveOp *op) {}
    virtual void Process(IRMoveNegatedOp *op) {}
    virtual void Process(IRSaturatingAddOp *op) {}
    virtual void Process(IRSaturatingSubtractOp *op) {}
    virtual void Process(IRMultiplyOp *op) {}
    virtual void Process(IRMultiplyLongOp *op) {}
    virtual void Process(IRAddLongOp *op) {}
    virtual void Process(IRStoreFlagsOp *op) {}
    virtual void Process(IRUpdateFlagsOp *op) {}
    virtual void Process(IRUpdateStickyOverflowOp *op) {}
    virtual void Process(IRBranchOp *op) {}
    virtual void Process(IRBranchExchangeOp *op) {}
    virtual void Process(IRLoadCopRegisterOp *op) {}
    virtual void Process(IRStoreCopRegisterOp *op) {}
    virtual void Process(IRConstantOp *op) {}
    virtual void Process(IRCopyVarOp *op) {}
    virtual void Process(IRGetBaseVectorAddressOp *op) {}

    void MarkDirty(bool dirty) {
        m_dirty |= dirty;
    }

private:
    bool m_dirty = false;
};

} // namespace armajitto::ir
