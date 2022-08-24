#pragma once

#include "optimizer_pass_base.hpp"

#include <bit>
#include <optional>
#include <vector>

namespace armajitto::ir {

// Coalesces host flags manipulation instructions.
//
// This optimization simplifies sequences of stflg instructions.
//
// The algorithm processes instructions that consume and store host flags and coalesces stflg instructions whenever
// possible.
//
// Assuming the following IR code fragment:
//  #  instruction
//  1  stflg.nz {}          ; updates host NZ flags
//  2  stflg.v {}           ; updates host V flags
//  3  ld $v0, r0
//  4  ld $v1, r1
//  5  stflg.cv {}          ; updates host CV flags
//  6  adc.nz $v1, $v0      ; consumes host C flag; updates host NZ flags
//  7  ld $v3, cpsr
//  8  ldflg.nzc $v4, $v3   ; consumes host NZCV flags
//  9  st cpsr, $v4
// 10  stflg.z {z}          ; updates host Z flags
// 11  stflg.cv {c}         ; updates host CV flags
//
// As noted in the comments, some instructions may consume host flags, others may update the flags, and a few might do
// both simultaneously. However, some host flags updates are replaced by future operations, and some stflg instructions
// could be merged into one.
//
//  #  instruction           flags consumed   updated
//  1  stflg.nz {}                            N:1 (overwritten by 6), Z:1 (overwritten by 6)
//  2  stflg.v {}                             V:2 (overwritten by 5)
//  3  ld $v0, r0
//  4  ld $v1, r1
//  5  stflg.cv {}                            C:5 (consumed by 6), V:5 (overwrites 2, overwritten by 11)
//  6  adc.nz $v1, $v0       C                N:6 (overwrites 1, consumed by 8), Z:6 (overwrites 1, consumed by 8)
//  7  ld $v3, cpsr
//  8  ldflg.nzc $v4, $v3    NZC
//  9  st cpsr, $v4
// 10  stflg.z {z}                            Z:10
// 11  stflg.cv {c}                           C:11, V:11 (overwrites 5)
//
// All overwritten host flags can be safely erased from the code. On this sequence, the optimizer performs the following
// actions:
// - Instructions 1 and 2 are removed as they no longer update any host flags.
// - Instruction 5 has its V flag overwritten by instruction 11, so it is removed from the instruction. It still updates
//   other host flags that are consumed by other instructions, so it is left alone after that change.
// - The flags output by instruction 6 are all consumed by instruction 8, so they remain intact.
// - Finally, the flags updated by instructions 10 and 11 are left untouched as they produce side effects (updating the
//   host flags).
//
// With this first stage complete, the code now looks like this:
//
//  #  instruction
//  1  ld $v0, r0
//  2  ld $v1, r1
//  3  stflg.c {}
//  4  adc.nz $v1, $v0
//  5  ld $v3, cpsr
//  6  ldflg.nzc $v4, $v3
//  7  st cpsr, $v4
//  8  stflg.z {z}
//  9  stflg.cv {c}
//
// Observe that it is possible to merge multiple stflg instructions if their flags are not consumed by any other
// instruction, which is the case with instructions 8 and 9. Whenever a sequence of stflg instructions are encountered,
// the flags mask and values are merged into the first stflg instruction that appeared in the sequence. In the example
// above, instruction 8 would be updated to also set the C flag and clear the V flag -- stflg.zcv {zc}.
// If multiple stflg instructions update the same flags in that sequence, the last one prevails. For example:
//
//  #  instruction        mask   values
//  1  stflg.nz {nz}      nz     nz
//  2  stflg.zc {}        nzc    n
//  3  stflg.nv {v}       nzcv      v
//  4  stflg.cq {cq}      nzcvq    cvq
//
// These instructions are merged into a single instruction:
//
//  #  instruction        mask     values
//  1  stflg.nzcvq {cvq}  nzcvq    cvq
class HostFlagsOpsCoalescenceOptimizerPass final : public OptimizerPassBase {
public:
    HostFlagsOpsCoalescenceOptimizerPass(Emitter &emitter);

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
    // StoreFlags flags tracking

    // Pointer to StoreFlags instruction that is being updated
    IRStoreFlagsOp *m_storeFlagsOp = nullptr;
};

} // namespace armajitto::ir
