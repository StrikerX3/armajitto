#pragma once

#include "dead_store_elimination_base.hpp"

#include <array>
#include <vector>

namespace armajitto::ir {

// Performs dead store elimination for host flags.
//
// This algorithm tracks the last instruction that updates the host state of each one of the five CPSR flags: NZCVQ.
// Whenever a flag is overwritten, the host flag update is removed from previous instruction that updated it. Flags that
// are consumed (read) by other instructions are preserved. A later optimization pass removes dead instructions that no
// longer write to any variables or flags.
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
// The algorithm takes the following actions for each instruction:
//  1-3. No action taken -- instructions don't read or write host flags.
//  4. This instruction is recorded as the last writer to NZCV host flags.
//  5. This instruction consumes the C host flag, so instruction 4 is no longer tracked for it. It also outputs NZCV,
//     replacing instruction 4 as the writer for flags NZV, which are removed from the previous instruction. This
//     instruction is now recorded as the last writer to NZCV host flags.
//  6. No action taken -- instructions don't read or write host flags.
//  7. This instruction overwrites the NZ flags written by instruction 5, so those flags are disabled from that
//     instruction, and this instruction is now recorded as the last writer to NZ flags.
//
// After those actions, the resulting code is:
//                              last writes per flag   actions taken
//  #  instruction
//  1  ld $v0, r1
//  2  ld $v1, r2
//  3  ld $v2, r3
//  4  add.c $v3, $v0, $v1      N:4, Z:4, C:4, V:4
//  5  adc.cv $v4, $v3, $v2     N:5, Z:5, C:5, V:5      consumed C, overwritten NZV
//  6  st r0, $v4               N:5, Z:5, C:5, V:5
//  7  stflg.nz {}              N:7, Z:7, C:5, V:5      overwritten NZ
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
