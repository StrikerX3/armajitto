#pragma once

#include "dead_store_elimination_base.hpp"

#include <array>
#include <vector>

namespace armajitto::ir {

// Performs dead store elimination for PSRs.
//
// TODO: write documentation
//
// TODO: describe further with examples
class DeadPSRStoreEliminationOptimizerPass final : public DeadStoreEliminationOptimizerPassBase {
public:
    DeadPSRStoreEliminationOptimizerPass(Emitter &emitter);

private:
    // void Process(IRGetRegisterOp *op) final;
    // void Process(IRSetRegisterOp *op) final;
    void Process(IRGetCPSROp *op) final;
    void Process(IRSetCPSROp *op) final;
    void Process(IRGetSPSROp *op) final;
    void Process(IRSetSPSROp *op) final;
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
    // void Process(IRStoreFlagsOp *op) final;
    void Process(IRLoadFlagsOp *op) final;
    void Process(IRLoadStickyOverflowOp *op) final;
    // void Process(IRBranchOp *op) final;
    // void Process(IRBranchExchangeOp *op) final;
    // void Process(IRLoadCopRegisterOp *op) final;
    // void Process(IRStoreCopRegisterOp *op) final;
    // void Process(IRConstantOp *op) final;
    void Process(IRCopyVarOp *op) final;
    // void Process(IRGetBaseVectorAddressOp *op) final;

    // -------------------------------------------------------------------------
    // PSR read and write tracking

    struct CPSRVar {
        Variable var;
        IROp *writeOp = nullptr;
    };

    uintmax_t m_cpsrVersion = 1;
    uintmax_t m_nextCPSRVersion = 2;
    std::vector<CPSRVar> m_cpsrVarMap;
    std::vector<uintmax_t> m_varCPSRVersionMap;
    std::array<IROp *, 32> m_spsrWrites{{nullptr}};

    bool RecordAndEraseDeadCPSRRead(VariableArg var, IROp *loadOp);
    void RecordCPSRWrite(VariableArg src, IROp *op);
    bool CheckAndEraseDeadCPSRLoadStore(IROp *loadOp);

    bool HasCPSRVersion(VariableArg var);
    bool HasCPSRVersion(VarOrImmArg var);
    void AssignNewCPSRVersion(VariableArg var);
    void CopyCPSRVersion(VariableArg dst, VariableArg src);

    void SubstituteCPSRVar(VariableArg &var);
    void SubstituteCPSRVar(VarOrImmArg &var);

    void ResizeCPSRToVarMap(size_t index);
    void ResizeVarToCPSRVersionMap(size_t swindexize);

    void RecordSPSRRead(arm::Mode mode);
    void RecordSPSRWrite(arm::Mode mode, IROp *op);
};

} // namespace armajitto::ir
