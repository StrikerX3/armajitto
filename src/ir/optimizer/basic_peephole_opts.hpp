#pragma once

#include "optimizer_pass_base.hpp"

#include <array>
#include <optional>
#include <vector>

namespace armajitto::ir {

// Applies a set of basic peephole optimizations to the code.
//
// Bitwise operation chaining
// --------------------------
// This optimization simplifies sequences of bitwise operations on a single chain of variables.
//
// The algorithm keeps track of the bits changed by each bitwise operation (AND, OR, BIC, XOR*) that operates on a
// variable and an immediate, or basic move and copy operations (MOV, COPY, MVN*), as long as these are the only
// operations to be applied to a value and they output no flags. For the MVN and XOR operations, all affected bits must
// be known -- MVN affects all bits, while XOR only affects bits set in the immediate value.
//
// Assuming the following IR code fragment:
//     instruction
//  1  mov $v0, r0  (r0 is an unknown value)
//  2  and $v1, $v0, #0xffff0000
//  3  orr $v2, $v1, #0xdead0000
//  4  bic $v3, $v2, #0x0000ffff
//  5  xor $v4, $v3, #0x0000beef
//  6  mov $v5, $v4
//  7  mvn $v6, $v5
//
// Due to the nature of bitwise operations, we can determine the exact values of affected bits after each operation.
// The algorithm tracks known and unknown values on a bit-by-bit basis for each variable in the sequence. As long as
// variables are consumed by the four bitwise operators, the algorithm can expand its knowledge of the value based on
// the operations performed:
//
//     instruction                 var  known mask  known values
//  1  mov $v0, (unknown)          $v0  0x00000000  0x........  (dots = don't matter, but they should be zeros)
//  2  and $v1, $v0, #0xffff0000   $v1  0xFFFF0000  0x0000....
//  3  orr $v2, $v1, #0xdead0000   $v2  0xFFFF0000  0xDEAD....
//  4  bic $v3, $v2, #0x0000ffff   $v3  0xFFFFFFFF  0xDEAD0000
//  5  xor $v4, $v3, #0x0000beef   $v4  0xFFFFFFFF  0xDEADBEEF
//  6  mov $v5, $v4                $v5  0xFFFFFFFF  0xDEADBEEF
//  7  mvn $v6, $v5, #0x0000beef   $v5  0xFFFFFFFF  0x21524110
//
// By instruction 5, we already know the entire value of the variable and can therefore begin replacing the instructions
// with constant assignments:
//
//     instruction                 var  known mask  known values  action
// ... ...                         ...  ...         ...
//  5  xor $v4, $v3, #0x0000beef   $v4  0xFFFFFFFF  0xDEADBEEF    replace -> const $v4, #0xdeadbeef
//  6  mov $v5, $v4                $v5  0xFFFFFFFF  0xDEADBEEF    replace -> const $v5, #0xdeadbeef
//  7  mvn $v6, $v5, #0x0000beef   $v5  0xFFFFFFFF  0x21524110    replace -> const $v6, #0x21524110
//
// The sequence is broken if any other instruction consumes the variable used in the chain, at which point the algorithm
// rewrites the whole sequence of instructions.
// If the entire value is known, the algorithm emits a simple const <last var>, <constant>.
// If only a few bits are known, the algorithm outputs a BIC and an ORR with the known zero and one bits, respectively,
// if there are any. For example:
//
//    known mask  known values  output sequence
//    0xFF00FF00  0xF0..0F..    bic <intermediate var>, <base var>,  0x0F00F000
//                              orr <final var>, <intermediate var>, 0xF0000F00
//    0xFF00FF00  0xFF..FF..    orr <final var>, <base var>, 0xFF00FF00
//    0xFF00FF00  0x00..00..    bic <final var>, <base var>, 0xFF00FF00
//
// Arithmetic simplification
// -------------------------
// For sequences of instructions that operate on variables with fully known values (as determined by the bitwise
// operation chaining technique above, or by constant assignments), all ALU operations can be applied. This is
// effectively a slightly more advanced form of constant propagation and folding, and as such, it is left to that pass.
//
// Sequences of ALU operations that act on unknown values, however, can sometimes be simplified.
//
// Assuming the following IR code fragment:
//     instruction
//  1  mov $v0, r0  (r0 is an unknown value)
//  2  add $v1, $v0, 3
//  3  sub $v2, $v1, 5
//  4  add $v3, $v2, 6
//
// It is clear that the final result in $v3 is equal to $v0 + 4. The unknown value is involved in a series of simple
// additions and subtractions, with no flags being output in any step of the calculation.
//
// This optimization is applied to any sequences of ADD, SUB and RSB with a variable and an immediate, as well as ADC
// and SBC if the carry flag is known.
class BasicPeepholeOptimizerPass final : public OptimizerPassBase {
public:
    BasicPeepholeOptimizerPass(Emitter &emitter)
        : OptimizerPassBase(emitter) {}

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
    // void Process(IRRotateRightExtendOp *op) final;
    void Process(IRBitwiseAndOp *op) final;
    void Process(IRBitwiseOrOp *op) final;
    void Process(IRBitwiseXorOp *op) final;
    void Process(IRBitClearOp *op) final;
    void Process(IRCountLeadingZerosOp *op) final;
    // void Process(IRAddOp *op) final;
    // void Process(IRAddCarryOp *op) final;
    // void Process(IRSubtractOp *op) final;
    // void Process(IRSubtractCarryOp *op) final;
    void Process(IRMoveOp *op) final;
    void Process(IRMoveNegatedOp *op) final;
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
    void Process(IRConstantOp *op) final;
    void Process(IRCopyVarOp *op) final;
    // void Process(IRGetBaseVectorAddressOp *op) final;

    // -------------------------------------------------------------------------
    // Value tracking

    struct Value {
        bool valid = false; // set to true if this value came from one of the bitwise, copy or constant ops
        uint32_t knownBits = 0;
        uint32_t value = 0;
    };

    // Value per variable
    std::vector<Value> m_values;

    void ResizeValues(size_t index);
    void AssignConstant(Variable var, uint32_t value);
    void CopyVariable(Variable var, Variable src);
    void DeriveKnownBits(Variable var, Variable src, uint32_t mask, uint32_t value);
};

} // namespace armajitto::ir
