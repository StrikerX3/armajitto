#pragma once

#include "dead_store_elimination_base.hpp"

#include <array>
#include <vector>

namespace armajitto::ir {

// Performs dead store elimination for host flags.
//
// TODO: write documentation
//
// TODO: describe further with examples
class DeadHostFlagStoreEliminationOptimizerPass final : public DeadStoreEliminationOptimizerPassBase {
public:
    DeadHostFlagStoreEliminationOptimizerPass(Emitter &emitter)
        : DeadStoreEliminationOptimizerPassBase(emitter) {}

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
    void Process(IRLogicalShiftLeftOp *op) final;
    void Process(IRLogicalShiftRightOp *op) final;
    void Process(IRArithmeticShiftRightOp *op) final;
    void Process(IRRotateRightOp *op) final;
    void Process(IRRotateRightExtendedOp *op) final;
    void Process(IRBitwiseAndOp *op) final;
    void Process(IRBitwiseOrOp *op) final;
    void Process(IRBitwiseXorOp *op) final;
    void Process(IRBitClearOp *op) final;
    // void Process(IRCountLeadingZerosOp *op) final;
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
    // void Process(IRBranchOp *op) final;
    // void Process(IRBranchExchangeOp *op) final;
    // void Process(IRLoadCopRegisterOp *op) final;
    // void Process(IRStoreCopRegisterOp *op) final;
    // void Process(IRConstantOp *op) final;
    // void Process(IRCopyVarOp *op) final;
    // void Process(IRGetBaseVectorAddressOp *op) final;

    // -------------------------------------------------------------------------
    // Host flag writes tracking

    // Writes: ALU instructions and store flags
    // Reads: Load flags
    IROp *m_hostFlagWriteN = nullptr;
    IROp *m_hostFlagWriteZ = nullptr;
    IROp *m_hostFlagWriteC = nullptr;
    IROp *m_hostFlagWriteV = nullptr;
    IROp *m_hostFlagWriteQ = nullptr;

    void RecordHostFlagsRead(arm::Flags flags);
    void RecordHostFlagsWrite(arm::Flags flags, IROp *op);

    // -------------------------------------------------------------------------
    // Generic EraseHostFlagWrite

    // Catch-all method for unused ops, required by the visitor
    template <typename T>
    void EraseHostFlagWrite(arm::Flags flag, T *op) {}

    void EraseHostFlagWrite(arm::Flags flag, IRLogicalShiftLeftOp *op);
    void EraseHostFlagWrite(arm::Flags flag, IRLogicalShiftRightOp *op);
    void EraseHostFlagWrite(arm::Flags flag, IRArithmeticShiftRightOp *op);
    void EraseHostFlagWrite(arm::Flags flag, IRRotateRightOp *op);
    void EraseHostFlagWrite(arm::Flags flag, IRRotateRightExtendedOp *op);
    void EraseHostFlagWrite(arm::Flags flag, IRBitwiseAndOp *op);
    void EraseHostFlagWrite(arm::Flags flag, IRBitwiseOrOp *op);
    void EraseHostFlagWrite(arm::Flags flag, IRBitwiseXorOp *op);
    void EraseHostFlagWrite(arm::Flags flag, IRBitClearOp *op);
    void EraseHostFlagWrite(arm::Flags flag, IRAddOp *op);
    void EraseHostFlagWrite(arm::Flags flag, IRAddCarryOp *op);
    void EraseHostFlagWrite(arm::Flags flag, IRSubtractOp *op);
    void EraseHostFlagWrite(arm::Flags flag, IRSubtractCarryOp *op);
    void EraseHostFlagWrite(arm::Flags flag, IRMoveOp *op);
    void EraseHostFlagWrite(arm::Flags flag, IRMoveNegatedOp *op);
    void EraseHostFlagWrite(arm::Flags flag, IRSaturatingAddOp *op);
    void EraseHostFlagWrite(arm::Flags flag, IRSaturatingSubtractOp *op);
    void EraseHostFlagWrite(arm::Flags flag, IRMultiplyOp *op);
    void EraseHostFlagWrite(arm::Flags flag, IRMultiplyLongOp *op);
    void EraseHostFlagWrite(arm::Flags flag, IRAddLongOp *op);
    void EraseHostFlagWrite(arm::Flags flag, IRStoreFlagsOp *op);
    void EraseHostFlagWrite(arm::Flags flag, IRLoadFlagsOp *op);
    void EraseHostFlagWrite(arm::Flags flag, IRLoadStickyOverflowOp *op);
};

} // namespace armajitto::ir
