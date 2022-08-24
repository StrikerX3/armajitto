#pragma once

#include "optimizer_pass_base.hpp"

namespace armajitto::ir {

// Base class for all dead store elimination optimization passes.
//
// This class provides common functionality for those passes, specifically:
// - Determining if an instruction is "dead" (has no writes or side effects)
class DeadStoreEliminationOptimizerPassBase : public OptimizerPassBase {
public:
    DeadStoreEliminationOptimizerPassBase(Emitter &emitter, bool backward = false)
        : OptimizerPassBase(emitter, backward) {}

protected:
    void PostProcess() final;

    virtual void PostProcessImpl() {}

    // -------------------------------------------------------------------------
    // Generic IsDeadInstruction
    // Determines if instructions have no writes or side effects

    // Catch-all method for unused ops, required by the visitor
    template <typename T>
    bool IsDeadInstruction(T *op) {
        return false;
    }

    bool IsDeadInstruction(IRGetRegisterOp *op);
    // IRSetRegisterOp has side effects
    bool IsDeadInstruction(IRGetCPSROp *op);
    // IRSetCPSROp has side effects
    bool IsDeadInstruction(IRGetSPSROp *op);
    // IRSetSPSROp has side effects
    bool IsDeadInstruction(IRMemReadOp *op);
    // IRMemWriteOp has side effects
    // IRPreloadOp has side effects
    bool IsDeadInstruction(IRLogicalShiftLeftOp *op);
    bool IsDeadInstruction(IRLogicalShiftRightOp *op);
    bool IsDeadInstruction(IRArithmeticShiftRightOp *op);
    bool IsDeadInstruction(IRRotateRightOp *op);
    bool IsDeadInstruction(IRRotateRightExtendedOp *op);
    bool IsDeadInstruction(IRBitwiseAndOp *op);
    bool IsDeadInstruction(IRBitwiseOrOp *op);
    bool IsDeadInstruction(IRBitwiseXorOp *op);
    bool IsDeadInstruction(IRBitClearOp *op);
    bool IsDeadInstruction(IRCountLeadingZerosOp *op);
    bool IsDeadInstruction(IRAddOp *op);
    bool IsDeadInstruction(IRAddCarryOp *op);
    bool IsDeadInstruction(IRSubtractOp *op);
    bool IsDeadInstruction(IRSubtractCarryOp *op);
    bool IsDeadInstruction(IRMoveOp *op);
    bool IsDeadInstruction(IRMoveNegatedOp *op);
    bool IsDeadInstruction(IRSaturatingAddOp *op);
    bool IsDeadInstruction(IRSaturatingSubtractOp *op);
    bool IsDeadInstruction(IRMultiplyOp *op);
    bool IsDeadInstruction(IRMultiplyLongOp *op);
    bool IsDeadInstruction(IRAddLongOp *op);
    bool IsDeadInstruction(IRStoreFlagsOp *op);
    bool IsDeadInstruction(IRLoadFlagsOp *op);
    bool IsDeadInstruction(IRLoadStickyOverflowOp *op);
    // IRBranchOp has side effects
    // IRBranchExchangeOp has side effects
    bool IsDeadInstruction(IRLoadCopRegisterOp *op);
    // IRStoreCopRegisterOp has side effects
    bool IsDeadInstruction(IRConstantOp *op);
    bool IsDeadInstruction(IRCopyVarOp *op);
    bool IsDeadInstruction(IRGetBaseVectorAddressOp *op);
};

} // namespace armajitto::ir
