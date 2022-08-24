#pragma once

#include "dead_store_elimination_base.hpp"

#include <array>
#include <vector>

namespace armajitto::ir {

// Performs dead store elimination for flag values in variables.
//
// TODO: write documentation
//
// TODO: describe further with examples
class DeadFlagValueStoreEliminationOptimizerPass final : public DeadStoreEliminationOptimizerPassBase {
public:
    DeadFlagValueStoreEliminationOptimizerPass(Emitter &emitter);

private:
    // void Process(IRGetRegisterOp *op) final;
    // void Process(IRSetRegisterOp *op) final;
    void Process(IRGetCPSROp *op) final;
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
    // void Process(IRRotateRightExtendedOp *op) final;
    void Process(IRBitwiseAndOp *op) final;
    void Process(IRBitwiseOrOp *op) final;
    // void Process(IRBitwiseXorOp *op) final;
    void Process(IRBitClearOp *op) final;
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
    // Flags tracking

    struct FlagWrites {
        Variable base; // the base variable from which this chain originates

        IROp *writerOpN = nullptr; // last instruction that wrote to N
        IROp *writerOpZ = nullptr; // last instruction that wrote to Z
        IROp *writerOpC = nullptr; // last instruction that wrote to C
        IROp *writerOpV = nullptr; // last instruction that wrote to V
        IROp *writerOpQ = nullptr; // last instruction that wrote to Q
    };

    std::vector<FlagWrites> m_flagWritesPerVar;

    void ResizeFlagWritesPerVar(size_t index);
    void InitFlagWrites(VariableArg base);
    void RecordFlagWrites(VariableArg dst, VariableArg src, arm::Flags flags, IROp *writerOp);

    // -------------------------------------------------------------------------
    // Generic EraseFlagWrite

    // Catch-all method for unused ops, required by the visitor
    template <typename T>
    void EraseFlagWrite(arm::Flags flag, T *op) {}

    void EraseFlagWrite(arm::Flags flag, IRBitwiseAndOp *op);
    void EraseFlagWrite(arm::Flags flag, IRBitwiseOrOp *op);
    void EraseFlagWrite(arm::Flags flag, IRBitClearOp *op);
    void EraseFlagWrite(arm::Flags flag, IRLoadFlagsOp *op);
    void EraseFlagWrite(arm::Flags flag, IRLoadStickyOverflowOp *op);
};

} // namespace armajitto::ir
