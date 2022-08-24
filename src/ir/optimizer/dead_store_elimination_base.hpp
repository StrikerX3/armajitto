#pragma once

#include "optimizer_pass_base.hpp"

namespace armajitto::ir {

// Base class for all dead store elimination optimization passes.
//
// This class provides common functionality for those passes, specifically:
// - Erase "dead" instructions (no writes to any variables or flags)
class DeadStoreEliminationOptimizerPassBase : public OptimizerPassBase {
public:
    DeadStoreEliminationOptimizerPassBase(Emitter &emitter);

protected:
    // -------------------------------------------------------------------------
    // Generic EraseDeadInstruction
    // Erases instructions if they have no additional writes or side effects

    // Catch-all method for unused ops, required by the visitor
    template <typename T>
    bool EraseDeadInstruction(T *op) {
        return false;
    }

    bool EraseDeadInstruction(IRGetRegisterOp *op);
    // IRSetRegisterOp has side effects
    bool EraseDeadInstruction(IRGetCPSROp *op);
    // IRSetCPSROp has side effects
    bool EraseDeadInstruction(IRGetSPSROp *op);
    // IRSetSPSROp has side effects
    bool EraseDeadInstruction(IRMemReadOp *op);
    // IRMemWriteOp has side effects
    // IRPreloadOp has side effects
    bool EraseDeadInstruction(IRLogicalShiftLeftOp *op);
    bool EraseDeadInstruction(IRLogicalShiftRightOp *op);
    bool EraseDeadInstruction(IRArithmeticShiftRightOp *op);
    bool EraseDeadInstruction(IRRotateRightOp *op);
    bool EraseDeadInstruction(IRRotateRightExtendedOp *op);
    bool EraseDeadInstruction(IRBitwiseAndOp *op);
    bool EraseDeadInstruction(IRBitwiseOrOp *op);
    bool EraseDeadInstruction(IRBitwiseXorOp *op);
    bool EraseDeadInstruction(IRBitClearOp *op);
    bool EraseDeadInstruction(IRCountLeadingZerosOp *op);
    bool EraseDeadInstruction(IRAddOp *op);
    bool EraseDeadInstruction(IRAddCarryOp *op);
    bool EraseDeadInstruction(IRSubtractOp *op);
    bool EraseDeadInstruction(IRSubtractCarryOp *op);
    bool EraseDeadInstruction(IRMoveOp *op);
    bool EraseDeadInstruction(IRMoveNegatedOp *op);
    bool EraseDeadInstruction(IRSaturatingAddOp *op);
    bool EraseDeadInstruction(IRSaturatingSubtractOp *op);
    bool EraseDeadInstruction(IRMultiplyOp *op);
    bool EraseDeadInstruction(IRMultiplyLongOp *op);
    bool EraseDeadInstruction(IRAddLongOp *op);
    bool EraseDeadInstruction(IRStoreFlagsOp *op);
    bool EraseDeadInstruction(IRLoadFlagsOp *op);
    bool EraseDeadInstruction(IRLoadStickyOverflowOp *op);
    // IRBranchOp has side effects
    // IRBranchExchangeOp has side effects
    bool EraseDeadInstruction(IRLoadCopRegisterOp *op);
    // IRStoreCopRegisterOp has side effects
    bool EraseDeadInstruction(IRConstantOp *op);
    bool EraseDeadInstruction(IRCopyVarOp *op);
    bool EraseDeadInstruction(IRGetBaseVectorAddressOp *op);
};

} // namespace armajitto::ir
