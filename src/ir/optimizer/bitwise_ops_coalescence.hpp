#pragma once

#include "optimizer_pass_base.hpp"

#include <bit>
#include <vector>

namespace armajitto::ir {

// Coalesces sequences of bitwise operations.
//
// This optimization simplifies sequences of bitwise operations on a chain of variables.
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
// variables are consumed by bitwise operators, the algorithm can expand its knowledge of the value based on the
// operations performed:
//
// (Note 1: dots are unknown digits, but internally they should be zeros)
// (Note 2: rotation is an offset for right rotation applied to the base value)
//
//     instruction                 var  known mask  known values  rotation (right)
//  1  mov $v0, (unknown)          $v0  0x00000000  0x........    0
//  2  and $v1, $v0, #0xfff0000f   $v1  0xFFF0000F  0x000....0    0
//  3  orr $v2, $v1, #0xead0000d   $v2  0xFFF0000F  0xEAD....D    0
//  4  bic $v3, $v2, #0x000ffff0   $v3  0xFFFFFFFF  0xEAD0000D    0
//  5  xor $v4, $v3, #0x000beef0   $v4  0xFFFFFFFF  0xEADBEEFD    0
//  6  mov $v5, $v4                $v5  0xFFFFFFFF  0xEADBEEFD    0
//  7  mvn $v6, $v5                $v6  0xFFFFFFFF  0x15241102    0
//  8  ror $v7, $v6, #0x4          $v7  0xFFFFFFFF  0x21524110    4
//
// By instruction 5, we already know the entire value of the variable and can therefore begin replacing the instructions
// with constant assignments:
//
//     instruction                 var  known mask  known values  action
// ... ...                         ...  ...         ...
//  5  xor $v4, $v3, #0x0000beef   $v4  0xFFFFFFFF  0xEADBEEFD    replace -> const $v4, #0xeadbeefd
//  6  mov $v5, $v4                $v5  0xFFFFFFFF  0xEADBEEFD    replace -> const $v5, #0xeadbeefd
//  7  mvn $v6, $v5, #0x0000beef   $v5  0xFFFFFFFF  0x15241102    replace -> const $v6, #0x15241102
//  8  ror $v7, $v6, #0x4          $v7  0xFFFFFFFF  0x21524110    replace -> const $v7, #0x21524110
//
// The sequence is broken if any other instruction consumes the variable used in the chain, at which point the algorithm
// rewrites the whole sequence of instructions.
//
// If the entire value is known, the algorithm emits a simple const <last var>, <constant>.
// If only a few bits are known, the algorithm outputs the following instructions, in this order:
// - LSR for the rotation, if it is non-zero and all <rotation offset> most significant bits are known
// - ROR for the rotation, if it is non-zero but some <rotation offset> most significant bits are unknown
// - ORR for all known ones, if any
// - BIC for all known zeros, if any
// - EOR for all flipped bits, if any
//
// For example:
//
//    known mask  known values  flipped bits  rotation  output sequence
//    0xFF00FF00  0xF0..0F..    0x00000000    0         orr <intermediate var>, <base var>, 0xF0000F00
//                                                      bic <final var>, <intermediate var>,  0x0F00F000
//    0xFF00FF00  0xFF..FF..    0x00000000    0         orr <final var>, <base var>, 0xFF00FF00
//    0xFF00FF00  0x00..00..    0x00000000    0         bic <final var>, <base var>, 0xFF00FF00
//    0xFF00FF00  0x00..00..    0x00FF00FF    0         bic <intermediate var>, <base var>, 0xFF00FF00
//                                                      eor <final var>, <intermediate var>, 0x00FF00FF
//    0xFF00FF00  0xFF..FF..    0x00000000    4         lsr <intermediate var>, <base var>, 4
//                                                      orr <final var>, <intermediate var>, 0xFF00FF00
//    0x0000FF00  0x....FF..    0x00000000    4         ror <intermediate var>, <base var>, 4
//                                                      orr <final var>, <intermediate var>, 0x0000FF00
//    0x0000FF00  0x....F0..    0x00FF0000    4         ror <intermediate var 1>, <base var>, 4
//                                                      orr <intermediate var 2> <intermediate var 1>, 0x0000F000
//                                                      bic <intermediate var 3>, <intermediate var 2>, 0x00000F00
//                                                      eor <final var>, <intermediate var 3>, 0x00FF0000
class BitwiseOpsCoalescenceOptimizerPass final : public OptimizerPassBase {
public:
    BitwiseOpsCoalescenceOptimizerPass(Emitter &emitter);

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

        bool trifecta;

        // These are checked when trifecta == false
        bool hasOnes;
        bool hasZeros;
        bool hasFlips;

        // These are checked when trifecta == true
        bool hasTrifectaClear = false;
        bool hasTrifectaFlip = false;

        // These are checked in both cases
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

        void operator()(IRLogicalShiftRightOp *op);
        void operator()(IRRotateRightOp *op);
        void operator()(IRBitwiseOrOp *op);
        void operator()(IRBitClearOp *op);
        void operator()(IRBitwiseXorOp *op);

        void CommonShiftCheck(VarOrImmArg &value, VarOrImmArg &amount, VariableArg dst);
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

    // -------------------------------------------------------------------------
    // Host carry flag state

    // Note: this pass assumes that the constant propagation optimization was applied beforehand.
    // If at any point we find an instruction that modifies the carry flag, we'll assume that the state is unknown.

    enum class FlagState { Unknown, Clear, Set };
    FlagState m_hostCarryFlagState = FlagState::Unknown;
};

} // namespace armajitto::ir
