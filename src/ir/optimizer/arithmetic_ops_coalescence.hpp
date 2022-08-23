#pragma once

#include "optimizer_pass_base.hpp"

#include <bit>
#include <optional>
#include <vector>

namespace armajitto::ir {

// Coalesces sequences of arithmetic operations.
//
// This optimization simplifies sequences of arithmetic operations on a chain of variables.
//
// The algorithm keeps track of the arithmetic operations that operate on a variable and an immediate as well as basic
// move and copy operations, chaining together the results and outputting a simplified sequence of operations.
//
// Assuming the following IR code fragment:
//     instruction
//  1  mov $v0, r0  (r0 is an unknown value)
//  2  add $v1, $v0, 3
//  3  sub $v2, $v1, 5
//  4  add $v3, $v2, 6
//  5  st r0, $v3
//
// It is clear that the final result in $v3 is equal to $v0 + 4 (3 - 5 + 6). The unknown value is involved in a series
// of simple additions and subtractions, with no flags being output in any step of the calculation.
//
// This optimization is applied to any sequences of ADD, SUB and RSB with a variable and an immediate, and also ADC, SBC
// and RSC if the carry flag is known. COPY, MOV and MVN are also optimized. It keeps a running sum of all operations,
// as well as any negations that may have been applied to the base variable.
//
// When the variable is the subtrahend of any subtraction operation, it is also negated, as well as any accumulated sum
// up to that point. MVN negates and subtracts one from the running sum.
class ArithmeticOpsCoalescenceOptimizerPass final : public OptimizerPassBase {
public:
    ArithmeticOpsCoalescenceOptimizerPass(Emitter &emitter)
        : OptimizerPassBase(emitter) {}

    ~ArithmeticOpsCoalescenceOptimizerPass() {
        // TODO: remove this; it's here to give the class a non-trivial destructor
    }

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
    // void Process(IRRotateRightExtendedOp *op) final;
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
    // void Process(IRLoadFlagsOp *op) final;
    // void Process(IRLoadStickyOverflowOp *op) final;
    // void Process(IRBranchOp *op) final;
    // void Process(IRBranchExchangeOp *op) final;
    // void Process(IRLoadCopRegisterOp *op) final;
    // void Process(IRStoreCopRegisterOp *op) final;
    // void Process(IRConstantOp *op) final;
    // void Process(IRCopyVarOp *op) final;
    // void Process(IRGetBaseVectorAddressOp *op) final;
};

} // namespace armajitto::ir
