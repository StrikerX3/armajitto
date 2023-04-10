#pragma once

#include "optimizer_pass_base.hpp"

namespace armajitto::ir {

// Optimizes variable lifetimes.
//
// (TODO: describe algorithm)
//
// (TODO: demonstrate algorithm with an example)
class VarLifetimeOptimizerPass final : public OptimizerPassBase {
public:
    VarLifetimeOptimizerPass(Emitter &emitter);

private:
    void PostProcess() final;

    void Process(IRGetRegisterOp *op) final;
    void Process(IRSetRegisterOp *op) final;
    void Process(IRGetCPSROp *op) final;
    void Process(IRSetCPSROp *op) final;
    void Process(IRGetSPSROp *op) final;
    void Process(IRSetSPSROp *op) final;
    void Process(IRMemReadOp *op) final;
    void Process(IRMemWriteOp *op) final;
    void Process(IRPreloadOp *op) final;
    void Process(IRLogicalShiftLeftOp *op) final;
    void Process(IRLogicalShiftRightOp *op) final;
    void Process(IRArithmeticShiftRightOp *op) final;
    void Process(IRRotateRightOp *op) final;
    void Process(IRRotateRightExtendedOp *op) final;
    void Process(IRBitwiseAndOp *op) final;
    void Process(IRBitwiseOrOp *op) final;
    void Process(IRBitwiseXorOp *op) final;
    void Process(IRBitClearOp *op) final;
    void Process(IRCountLeadingZerosOp *op) final;
    void Process(IRAddOp *op) final;
    void Process(IRAddCarryOp *op) final;
    void Process(IRSubtractOp *op) final;
    void Process(IRSubtractCarryOp *op) final;
    void Process(IRMoveOp *op) final;
    void Process(IRMoveNegatedOp *op) final;
    void Process(IRSaturatingAddOp *op) final;
    void Process(IRSaturatingSubtractOp *op) final;
    void Process(IRMultiplyOp *op) final;
    void Process(IRMultiplyLongOp *op) final;
    void Process(IRAddLongOp *op) final;
    void Process(IRStoreFlagsOp *op) final;
    void Process(IRLoadFlagsOp *op) final;
    void Process(IRLoadStickyOverflowOp *op) final;
    void Process(IRBranchOp *op) final;
    void Process(IRBranchExchangeOp *op) final;
    void Process(IRLoadCopRegisterOp *op) final;
    void Process(IRStoreCopRegisterOp *op) final;
    void Process(IRConstantOp *op) final;
    void Process(IRCopyVarOp *op) final;
    void Process(IRGetBaseVectorAddressOp *op) final;

    // -------------------------------------------------------------------------
    // Read/write tracking

    // TODO:
    // - Variables
    // - GPRs
    // - PSRs
    // - Host flags

    void RecordRead(VarOrImmArg arg);
    void RecordRead(VariableArg arg);
    void RecordRead(GPRArg arg);
    void RecordCPSRRead();
    void RecordSPSRRead(arm::Mode mode);
    void RecordPSRRead(size_t index);
    void RecordRead(arm::Flags flags);

    void RecordWrite(VarOrImmArg arg);
    void RecordWrite(VariableArg arg);
    void RecordWrite(GPRArg arg);
    void RecordCPSRWrite();
    void RecordSPSRWrite(arm::Mode mode);
    void RecordPSRWrite(size_t index);
    void RecordWrite(arm::Flags flags);
};

} // namespace armajitto::ir
