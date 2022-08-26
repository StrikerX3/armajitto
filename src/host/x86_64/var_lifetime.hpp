#pragma once

#include "armajitto/ir/basic_block.hpp"
#include "armajitto/ir/ir_ops.hpp"

#include <vector>

namespace armajitto::x86_64 {

class VarLifetimeTracker {
public:
    void Analyze(const ir::BasicBlock &block);

    bool IsEndOfLife(ir::Variable var, const ir::IROp *op) const;

private:
    std::vector<const ir::IROp *> m_lastVarReadOps;

    void SetLastVarReadOp(const ir::VariableArg &arg, const ir::IROp *op);
    void SetLastVarReadOp(const ir::VarOrImmArg &arg, const ir::IROp *op);

    template <typename T>
    void ComputeVarLifetimes(const T *op) {}

    void ComputeVarLifetimes(const ir::IRSetRegisterOp *op);
    void ComputeVarLifetimes(const ir::IRSetCPSROp *op);
    void ComputeVarLifetimes(const ir::IRSetSPSROp *op);
    void ComputeVarLifetimes(const ir::IRMemReadOp *op);
    void ComputeVarLifetimes(const ir::IRMemWriteOp *op);
    void ComputeVarLifetimes(const ir::IRPreloadOp *op);
    void ComputeVarLifetimes(const ir::IRLogicalShiftLeftOp *op);
    void ComputeVarLifetimes(const ir::IRLogicalShiftRightOp *op);
    void ComputeVarLifetimes(const ir::IRArithmeticShiftRightOp *op);
    void ComputeVarLifetimes(const ir::IRRotateRightOp *op);
    void ComputeVarLifetimes(const ir::IRRotateRightExtendedOp *op);
    void ComputeVarLifetimes(const ir::IRBitwiseAndOp *op);
    void ComputeVarLifetimes(const ir::IRBitwiseOrOp *op);
    void ComputeVarLifetimes(const ir::IRBitwiseXorOp *op);
    void ComputeVarLifetimes(const ir::IRBitClearOp *op);
    void ComputeVarLifetimes(const ir::IRCountLeadingZerosOp *op);
    void ComputeVarLifetimes(const ir::IRAddOp *op);
    void ComputeVarLifetimes(const ir::IRAddCarryOp *op);
    void ComputeVarLifetimes(const ir::IRSubtractOp *op);
    void ComputeVarLifetimes(const ir::IRSubtractCarryOp *op);
    void ComputeVarLifetimes(const ir::IRMoveOp *op);
    void ComputeVarLifetimes(const ir::IRMoveNegatedOp *op);
    void ComputeVarLifetimes(const ir::IRSaturatingAddOp *op);
    void ComputeVarLifetimes(const ir::IRSaturatingSubtractOp *op);
    void ComputeVarLifetimes(const ir::IRMultiplyOp *op);
    void ComputeVarLifetimes(const ir::IRMultiplyLongOp *op);
    void ComputeVarLifetimes(const ir::IRAddLongOp *op);
    void ComputeVarLifetimes(const ir::IRStoreFlagsOp *op);
    void ComputeVarLifetimes(const ir::IRLoadFlagsOp *op);
    void ComputeVarLifetimes(const ir::IRLoadStickyOverflowOp *op);
    void ComputeVarLifetimes(const ir::IRBranchOp *op);
    void ComputeVarLifetimes(const ir::IRBranchExchangeOp *op);
    void ComputeVarLifetimes(const ir::IRStoreCopRegisterOp *op);
    void ComputeVarLifetimes(const ir::IRCopyVarOp *op);
};

} // namespace armajitto::x86_64
