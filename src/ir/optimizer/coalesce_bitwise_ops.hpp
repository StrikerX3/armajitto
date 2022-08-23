#pragma once

#include "optimizer_pass_base.hpp"

#include <bit>
#include <optional>
#include <vector>

namespace armajitto::ir {

// Coalesces a sequence of bitwise operations.
//
// This optimization simplifies sequences of bitwise operations on a single chain of variables.
//
// The algorithm keeps track of the bits changed by each bitwise operation (AND, OR, BIC, XOR, LSL, LSR, ASR, ROR, RRX)
// that operates on a variable and an immediate, or basic move and copy operations (MOV, COPY, MVN), as long as these
// are the only operations to be applied to a value and they output no flags.
//
// Certain instructions have additional requirements for this optimization:
// - The MVN and XOR operations require all affected bits to be known. MVN affects all bits, while XOR only affects bits
//   set in the immediate value.
// - ASR requires the most significant bit to be known.
// - RRX requires the carry flag to be known.
//
// Assuming the following IR code fragment:
//     instruction
//  1  mov $v0, r0  (r0 is an unknown value)
//  2  and $v1, $v0, #0x0000ffff
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
// Shift and rotations are combined to apply a rotation to the base value before modifying the known bits.
class CoalesceBitwiseOpsOptimizerPass final : public OptimizerPassBase {
public:
    CoalesceBitwiseOpsOptimizerPass(Emitter &emitter)
        : OptimizerPassBase(emitter) {}

private:
    // void Process(IRGetRegisterOp *op) final;
    void Process(IRSetRegisterOp *op) final;
    // void Process(IRGetCPSROp *op) final;
    void Process(IRSetCPSROp *op) final;
    // void Process(IRGetSPSROp *op) final;
    void Process(IRSetSPSROp *op) final;
    void Process(IRMemReadOp *op) final;
    void Process(IRMemWriteOp *op) final;
    void Process(IRPreloadOp *op) final;
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
    void Process(IRStoreFlagsOp *op) final;
    void Process(IRLoadFlagsOp *op) final;
    void Process(IRLoadStickyOverflowOp *op) final;
    void Process(IRBranchOp *op) final;
    void Process(IRBranchExchangeOp *op) final;
    // void Process(IRLoadCopRegisterOp *op) final;
    void Process(IRStoreCopRegisterOp *op) final;
    void Process(IRConstantOp *op) final;
    void Process(IRCopyVarOp *op) final;
    // void Process(IRGetBaseVectorAddressOp *op) final;

    // -------------------------------------------------------------------------
    // Value tracking

    struct Value {
        bool valid = false;
        uint32_t knownBitsMask = 0;
        uint32_t knownBitsValue = 0;
        uint32_t flippedBits = 0;  // XOR or MVN; for unknown bits only
        uint32_t rotateOffset = 0; // LSL, LSR, ASR, ROR and RRX; rotate right, clamped to 0..31

        IROp *writerOp = nullptr; // pointer to the instruction that produced this variable
        Variable source;          // original source of the value for this variable
        Variable prev;            // previous variable from which this was derived

        uint32_t Ones() const {
            return knownBitsValue & knownBitsMask;
        }

        uint32_t Zeros() const {
            return ~knownBitsValue & knownBitsMask;
        }

        uint32_t Flips() const {
            return flippedBits & ~knownBitsMask;
        }

        uint32_t RotateOffset() const {
            return rotateOffset;
        }

        void Set(uint32_t bits) {
            knownBitsMask |= bits;
            knownBitsValue |= bits;
            flippedBits &= ~bits;
        }

        void Clear(uint32_t bits) {
            knownBitsMask |= bits;
            knownBitsValue &= ~bits;
            flippedBits &= ~bits;
        }

        void Flip(uint32_t bits) {
            knownBitsValue ^= bits & knownBitsMask;
            flippedBits ^= bits & ~knownBitsMask;
        }

        void LogicalShiftLeft(uint32_t amount) {
            if (amount >= 32u) {
                knownBitsMask = ~0u;
                knownBitsValue = 0u;
                flippedBits = 0u;
                rotateOffset = 0u;
            } else {
                const uint32_t zeros = ~(~0u << amount);
                knownBitsMask = (knownBitsMask << amount) | zeros;
                knownBitsValue <<= amount;
                flippedBits <<= amount;
                rotateOffset = (rotateOffset - amount) & 31u;
            }
        }

        void LogicalShiftRight(uint32_t amount) {
            if (amount >= 32u) {
                knownBitsMask = ~0u;
                knownBitsValue = 0u;
                flippedBits = 0u;
                rotateOffset = 0u;
            } else {
                const uint32_t zeros = ~(~0u >> amount);
                knownBitsMask = (knownBitsMask >> amount) | zeros;
                knownBitsValue >>= amount;
                flippedBits >>= amount;
                rotateOffset = (rotateOffset + amount) & 31u;
            }
        }

        // Returns true if the sign bit is known and the shift was applied
        bool ArithmeticShiftRight(uint32_t amount) {
            // Most significant bit must be known
            if ((knownBitsMask & (1u << 31u)) == 0) {
                return false;
            }

            if (amount >= 32u) {
                knownBitsMask = ~0u;
                knownBitsValue = static_cast<int32_t>(knownBitsValue) >> 31;
                flippedBits = 0u;
                rotateOffset = 0u;
            } else {
                const uint32_t mask = ~(~0u >> amount);
                knownBitsMask = (knownBitsMask >> amount) | mask;
                knownBitsValue = static_cast<int32_t>(knownBitsValue) >> amount;
                flippedBits >>= amount;
                rotateOffset = (rotateOffset + amount) & 31u;
            }
            return true;
        }

        void RotateRight(uint32_t amount) {
            amount &= 31u;
            knownBitsMask = std::rotr(knownBitsMask, amount);
            knownBitsValue = std::rotr(knownBitsValue, amount);
            flippedBits = std::rotr(flippedBits, amount);
            rotateOffset = (rotateOffset + amount) & 31u;
        }

        void RotateRightExtended(bool carry) {
            const uint32_t msb = (1u << 31u);
            knownBitsMask = std::rotr(knownBitsMask, 1u) | msb;
            knownBitsValue = std::rotr(knownBitsValue, 1u);
            if (carry) {
                knownBitsValue |= msb;
            } else {
                knownBitsValue &= ~msb;
            }
            flippedBits = std::rotr(flippedBits, 1u) & ~msb;
            rotateOffset = (rotateOffset + 1u) & 31u;
        }
    };

    // Value per variable
    std::vector<Value> m_values;

    void ResizeValues(size_t index);

    void AssignConstant(VariableArg var, uint32_t value);
    void CopyVariable(VariableArg var, VariableArg src, IROp *op);
    Value *DeriveKnownBits(VariableArg var, VariableArg src, IROp *op);

    Value *GetValue(VariableArg var);

    void ConsumeValue(VariableArg &var);
    void ConsumeValue(VarOrImmArg &var);

    // Helper struct to evaluate a sequence of values to check if they contain ROR, ORR, BIC and XOR instructions
    // matching the ones, zeros and flip bits as well as the rotation offset and input and output variables from the
    // given value.
    struct BitwiseOpsMatchState {
        bool valid = true;
        bool hasOnes;
        bool hasZeros;
        bool hasFlips;
        bool hasRotate;
        bool inputMatches = false;
        bool outputMatches = false;

        bool first = true;

        const uint32_t ones;
        const uint32_t zeros;
        const uint32_t flips;
        const uint32_t rotate;
        const Variable expectedInput;
        const Variable expectedOutput;

        const std::vector<Value> &values;

        BitwiseOpsMatchState(Value &value, Variable expectedOutput, const std::vector<Value> &values);

        bool Check(const Value *value);

        bool Valid() const;

        template <typename T>
        void operator()(T *) {
            valid = false;
        }

        void operator()(IRBitwiseOrOp *op);
        void operator()(IRBitClearOp *op);
        void operator()(IRBitwiseXorOp *op);
        void operator()(IRRotateRightOp *op);

        void CommonCheck(bool &flag, uint32_t matchValue, VarOrImmArg &lhs, VarOrImmArg &rhs, VariableArg dst);

        void CheckInputVar(Variable var);
        void CheckOutputVar(Variable var);
    };

    // -------------------------------------------------------------------------
    // Variable substitutions

    std::vector<Variable> m_varSubsts;

    void ResizeVarSubsts(size_t index);
    void Assign(VariableArg dst, VariableArg src);
    void Substitute(VariableArg &var);
    void Substitute(VarOrImmArg &var);
};

} // namespace armajitto::ir
