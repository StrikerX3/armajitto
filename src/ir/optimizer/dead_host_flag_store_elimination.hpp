#pragma once

#include "dead_store_elimination_base.hpp"

#include <array>

namespace armajitto::ir {

// Performs dead store elimination for host flags.
//
// This algorithm scans the code backwards, tracking the host state of each one of the five CPSR flags: NZCVQ.
// Whenever a host flag write is encountered, it is marked as "final" and any subsequent (previous) writes to those
// flags are erased. Reads reset the state of the affected flags. For instructions that simultaneously read and write
// such as ADC and SBC, the writes are processed before the reads. Dead instructions (that is, instructions that write
// to no variables or flags and have no side effects) are not processed. This can also happen with simultaneous read and
// write instructions if the write stage happens to modify the instruction such that it becomes dead -- the read stage
// is not processed.
//
// Assuming the following IR code fragment:
//  #  instruction
//  1  ld $v0, r1
//  2  ld $v1, r2
//  3  ld $v2, r3
//  4  add.nzcv $v3, $v0, $v1
//  5  adc.nzcv $v4, $v3, $v2
//  6  st r0, $v4
//  7  stflg.nz {}
//
// The algorithm takes the following actions for each instruction (note the backward scan order):
//  7. Write stage: Mark the NZ flags as final. No read stage.
//  6. No action taken.
//  5. Write stage: Remove NZ flags from the instruction's mask and mark CV flags as final.
//     Read stage: Mark C flag as unwritten. Current final mask is NZV.
//  4. Write stage: Remove NZV flags and mark C as final.
//  3-1. No action taken.
//
// After those actions, the resulting code is:
//  #  instruction
//  1  ld $v0, r1
//  2  ld $v1, r2
//  3  ld $v2, r3
//  4  add.c $v3, $v0, $v1
//  5  adc.cv $v4, $v3, $v2
//  6  st r0, $v4
//  7  stflg.nz {}
class DeadHostFlagStoreEliminationOptimizerPass final : public DeadStoreEliminationOptimizerPassBase {
public:
    DeadHostFlagStoreEliminationOptimizerPass(Emitter &emitter)
        : DeadStoreEliminationOptimizerPassBase(emitter, true) {}

private:
    void Reset() final;

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

    arm::Flags m_writtenFlags = arm::Flags::None;

    void RecordHostFlagsRead(arm::Flags flags, IROp *op);
    void RecordHostFlagsWrite(arm::Flags flags, IROp *op);

    // -------------------------------------------------------------------------
    // Generic EraseHostFlagsWrite

    // Catch-all method for unused ops, required by the visitor
    template <typename T>
    void EraseHostFlagsWrite(arm::Flags flags, T *op) {}

    void EraseHostFlagsWrite(arm::Flags flags, IRLogicalShiftLeftOp *op);
    void EraseHostFlagsWrite(arm::Flags flags, IRLogicalShiftRightOp *op);
    void EraseHostFlagsWrite(arm::Flags flags, IRArithmeticShiftRightOp *op);
    void EraseHostFlagsWrite(arm::Flags flags, IRRotateRightOp *op);
    void EraseHostFlagsWrite(arm::Flags flags, IRRotateRightExtendedOp *op);
    void EraseHostFlagsWrite(arm::Flags flags, IRBitwiseAndOp *op);
    void EraseHostFlagsWrite(arm::Flags flags, IRBitwiseOrOp *op);
    void EraseHostFlagsWrite(arm::Flags flags, IRBitwiseXorOp *op);
    void EraseHostFlagsWrite(arm::Flags flags, IRBitClearOp *op);
    void EraseHostFlagsWrite(arm::Flags flags, IRAddOp *op);
    void EraseHostFlagsWrite(arm::Flags flags, IRAddCarryOp *op);
    void EraseHostFlagsWrite(arm::Flags flags, IRSubtractOp *op);
    void EraseHostFlagsWrite(arm::Flags flags, IRSubtractCarryOp *op);
    void EraseHostFlagsWrite(arm::Flags flags, IRMoveOp *op);
    void EraseHostFlagsWrite(arm::Flags flags, IRMoveNegatedOp *op);
    void EraseHostFlagsWrite(arm::Flags flags, IRSaturatingAddOp *op);
    void EraseHostFlagsWrite(arm::Flags flags, IRSaturatingSubtractOp *op);
    void EraseHostFlagsWrite(arm::Flags flags, IRMultiplyOp *op);
    void EraseHostFlagsWrite(arm::Flags flags, IRMultiplyLongOp *op);
    void EraseHostFlagsWrite(arm::Flags flags, IRAddLongOp *op);
    void EraseHostFlagsWrite(arm::Flags flags, IRStoreFlagsOp *op);
    void EraseHostFlagsWrite(arm::Flags flags, IRLoadFlagsOp *op);
    void EraseHostFlagsWrite(arm::Flags flags, IRLoadStickyOverflowOp *op);
};

} // namespace armajitto::ir
