#pragma once

#include "common/host_flags_tracking.hpp"
#include "common/var_subst.hpp"
#include "optimizer_pass_base.hpp"

#include <bit>
#include <optional>
#include <vector>

namespace armajitto::ir {

// Eliminates identity operations.
//
// This optimization removes the following operations from the code if they don't output flags:
//   lsl   <var>, <var>, 0
//   lsr   <var>, <var>, 0
//   asr   <var>, <var>, 0
//   ror   <var>, <var>, 0
//   and   <var>, <var>, 0xFFFFFFFF
//   and   <var>, 0xFFFFFFFF, <var>
//   orr   <var>, <var>, 0
//   orr   <var>, 0, <var>
//   eor   <var>, <var>, 0
//   eor   <var>, 0, <var>
//   bic   <var>, <var>, 0
//   bic   <var>, 0, <var>
//   add   <var>, <var>, 0
//   add   <var>, 0, <var>
//   sub   <var>, <var>, 0
//   adc   <var>, <var>, 0 (with known ~C)
//   adc   <var>, 0, <var> (with known ~C)
//   sbc   <var>, <var>, 0 (with known  C)
//   qadd  <var>, <var>, 0
//   qadd  <var>, 0, <var>
//   qsub  <var>, <var>, 0
//   umul  <var>, <var>, 1
//   umul  <var>, 1, <var>
//   smul  <var>, <var>, 1
//   smul  <var>, 1, <var>
//   umull <var>, <var>:<var>, 0:1
//   umull <var>, 0:1, <var>:<var>
//   smull <var>, <var>:<var>, 0:1
//   smull <var>, 0:1, <var>:<var>
//   addl  <var>:<var>, <var>:<var>, 0:0
//   addl  <var>:<var>, 0:0, <var>:<var>
//
// The algorithm maps the output variables of removed instructions to the argument variables and substitutes all
// instances of those variables in subsequent instructions.
class IdentityOpsEliminationOptimizerPass final : public OptimizerPassBase {
public:
    IdentityOpsEliminationOptimizerPass(Emitter &emitter);

private:
    void PreProcess(IROp *op) final;
    void PostProcess(IROp *op) final;

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
    // void Process(IRRotateRightExtendedOp *op) final;
    void Process(IRBitwiseAndOp *op) final;
    void Process(IRBitwiseOrOp *op) final;
    void Process(IRBitwiseXorOp *op) final;
    void Process(IRBitClearOp *op) final;
    // void Process(IRCountLeadingZerosOp *op) final;
    void Process(IRAddOp *op) final;
    void Process(IRAddCarryOp *op) final;
    void Process(IRSubtractOp *op) final;
    void Process(IRSubtractCarryOp *op) final;
    // void Process(IRMoveOp *op) final;
    // void Process(IRMoveNegatedOp *op) final;
    void Process(IRSaturatingAddOp *op) final;
    void Process(IRSaturatingSubtractOp *op) final;
    void Process(IRMultiplyOp *op) final;
    void Process(IRMultiplyLongOp *op) final;
    void Process(IRAddLongOp *op) final;
    // void Process(IRStoreFlagsOp *op) final;
    // void Process(IRLoadFlagsOp *op) final;
    // void Process(IRLoadStickyOverflowOp *op) final;
    // void Process(IRBranchOp *op) final;
    // void Process(IRBranchExchangeOp *op) final;
    // void Process(IRLoadCopRegisterOp *op) final;
    // void Process(IRStoreCopRegisterOp *op) final;
    // void Process(IRConstantOp *op) final;
    // void Process(IRCopyVarOp *op) final;
    // void Process(IRGetBaseVectorAddressOp *op) final;

    // -------------------------------------------------------------------------
    // Common processors

    void ProcessShift(const VariableArg &dst, const VarOrImmArg &value, const VarOrImmArg &amount, bool setCarry,
                      IROp *op);
    void ProcessImmVarPair(const VariableArg &dst, const VarOrImmArg &lhs, const VarOrImmArg &rhs, arm::Flags flags,
                           uint32_t identityValue, IROp *op);

    // -------------------------------------------------------------------------
    // Helpers

    VarSubstitutor m_varSubst;
    HostFlagStateTracker m_hostFlagsStateTracker;
};

} // namespace armajitto::ir
