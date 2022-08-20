#pragma once

#include "optimizer_pass_base.hpp"

namespace armajitto::ir {

// Performs dead store elimination for variables, registers, PSRs and flags.
//
// TODO: describe algorithm, include an example
class DeadStoreEliminationOptimizerPass final : public OptimizerPassBase {
public:
    DeadStoreEliminationOptimizerPass(Emitter &emitter)
        : OptimizerPassBase(emitter) {}

private:
    // void Process(IRGetRegisterOp *op) final;
    // void Process(IRSetRegisterOp *op) final;
    // void Process(IRGetCPSROp *op) final;
    // void Process(IRSetCPSROp *op) final;
    // void Process(IRGetSPSROp *op) final;
    // void Process(IRSetSPSROp *op) final;
    // void Process(IRMemReadOp *op) final;
    // void Process(IRMemWriteOp *op) final;
    // void Process(IRPreloadOp *op) final;
    // void Process(IRLogicalShiftLeftOp *op) final;
    // void Process(IRLogicalShiftRightOp *op) final;
    // void Process(IRArithmeticShiftRightOp *op) final;
    // void Process(IRRotateRightOp *op) final;
    // void Process(IRRotateRightExtendOp *op) final;
    // void Process(IRBitwiseAndOp *op) final;
    // void Process(IRBitwiseOrOp *op) final;
    // void Process(IRBitwiseXorOp *op) final;
    // void Process(IRBitClearOp *op) final;
    // void Process(IRCountLeadingZerosOp *op) final;
    // void Process(IRAddOp *op) final;
    // void Process(IRAddCarryOp *op) final;
    // void Process(IRSubtractOp *op) final;
    // void Process(IRSubtractCarryOp *op) final;
    // void Process(IRMoveOp *op) final;
    // void Process(IRMoveNegatedOp *op) final;
    // void Process(IRSaturatingAddOp *op) final;
    // void Process(IRSaturatingSubtractOp *op) final;
    // void Process(IRMultiplyOp *op) final;
    // void Process(IRMultiplyLongOp *op) final;
    // void Process(IRAddLongOp *op) final;
    // void Process(IRStoreFlagsOp *op) final;
    // void Process(IRUpdateFlagsOp *op) final;
    // void Process(IRUpdateStickyOverflowOp *op) final;
    // void Process(IRBranchOp *op) final;
    // void Process(IRBranchExchangeOp *op) final;
    // void Process(IRLoadCopRegisterOp *op) final;
    // void Process(IRStoreCopRegisterOp *op) final;
    // void Process(IRConstantOp *op) final;
    // void Process(IRCopyVarOp *op) final;
    // void Process(IRGetBaseVectorAddressOp *op) final;
};

} // namespace armajitto::ir
