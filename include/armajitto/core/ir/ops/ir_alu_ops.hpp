#pragma once

#include "armajitto/core/ir/defs/arg_refs.hpp"
#include "ir_ops_base.hpp"

#include <optional>

namespace armajitto::ir {

// [s] = updates host flags

namespace detail {
    // Base type of bit shifting ALU operations.
    //   [op][s] <var:dst>, <var/imm:value>, <var/imm:amount>
    struct IRShiftOpBase : public IROpBase {
        VariableArg dst;
        VarOrImmArg value;
        VarOrImmArg amount;
        bool setFlags;
    };

    // Base type of binary ALU operations with optional result, which is used for comparison instructions.
    //   [op][s] <var?:dst>, <var/imm:lhs>, <var/imm:rhs>
    struct IRComparisonOpBase : public IROpBase {
        std::optional<VariableArg> dst;
        VarOrImmArg lhs;
        VarOrImmArg rhs;
        bool setFlags;
    };

    // Base type of binary ALU operations.
    //   [op][s] <var:dst>, <var/imm:lhs>, <var/imm:rhs>
    struct IRBinaryOpBase : public IROpBase {
        VariableArg dst;
        VarOrImmArg lhs;
        VarOrImmArg rhs;
        bool setFlags;
    };

    // Base type of saturating binary ALU operations.
    //   [op][s] <var:dst>, <var/imm:lhs>, <var/imm:rhs>
    struct IRSaturatingBinaryOpBase : public IROpBase {
        VariableArg dst;
        VarOrImmArg lhs;
        VarOrImmArg rhs;
    };

    // Base type of unary ALU operations.
    //   [op][s] <var:dst>, <var/imm:value>
    struct IRUnaryOpBase : public IROpBase {
        VariableArg dst;
        VarOrImmArg value;
        bool setFlags;
    };
} // namespace detail

// -----------------------------------------------------------------------------

// Logical shift left
//   lsl[s]   <var:dst>, <var/imm:value>, <var/imm:amount>
//
// Shifts bits in <value> left by <amount>, shifting in zeros, and stores the result in <dst>.
// Updates host flags if [s] is specified.
struct IRLogicalShiftLeftOp : public detail::IRShiftOpBase {
    static constexpr auto kOpcodeType = IROpcodeType::LogicalShiftLeft;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }
};

// Logical shift right
//   lsr[s]   <var:dst>, <var/imm:value>, <var/imm:amount>
//
// Shifts bits in value <right> by <amount>, shifting in zeros, and stores the result in <dst>.
// Updates host flags if [s] is specified.
struct IRLogicalShiftRightOp : public detail::IRShiftOpBase {
    static constexpr auto kOpcodeType = IROpcodeType::LogicalShiftRight;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }
};

// Arithmetic shift right
//   asr[s]   <var:dst>, <var/imm:value>, <var/imm:amount>
//
// Shifts bits in <value> right by <amount>, shifting in the sign bit of <value>, and stores the result in <dst>.
// Updates host flags if [s] is specified.
struct IRArithmeticShiftRightOp : public detail::IRShiftOpBase {
    static constexpr auto kOpcodeType = IROpcodeType::ArithmeticShiftRight;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }
};

// Rotate right
//   ror[s]   <var:dst>, <var/imm:value>, <var/imm:amount>
//
// Rotates bits in <value> right by <amount> and stores the result in <dst>.
// Updates host flags if [s] is specified.
struct IRRotateRightOp : public detail::IRShiftOpBase {
    static constexpr auto kOpcodeType = IROpcodeType::RotateRight;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }
};

// Rotate right extend
//   rrx[s]   <var:dst>, <var/imm:value>
//
// Rotates bits in <value> right by one, shifting in the carry flag, and stores the result in <dst>.
// Updates host flags if [s] is specified.
struct IRRotateRightExtendOp : public detail::IRShiftOpBase {
    static constexpr auto kOpcodeType = IROpcodeType::RotateRightExtend;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }
};

// Bitwise AND
//   and[s]   <var?:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> AND <rhs> and stores the result in <dst> if present.
// The TST operation omits <dst>.
// Updates host flags if [s] is specified.
struct IRBitwiseAndOp : public detail::IRComparisonOpBase {
    static constexpr auto kOpcodeType = IROpcodeType::BitwiseAnd;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }
};

// Bitwise XOR
//   eor[s]   <var?:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> XOR <rhs> and stores the result in <dst> if present.
// The TEQ operation omits <dst>.
// Updates host flags if [s] is specified.
struct IRBitwiseXorOp : public detail::IRComparisonOpBase {
    static constexpr auto kOpcodeType = IROpcodeType::BitwiseXor;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }
};

// Subtract
//   sub[s]   <var?:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> - <rhs> and stores the result in <dst> if present.
// The CMP operation omits <dst>.
// Updates host flags if [s] is specified.
struct IRSubtractOp : public detail::IRComparisonOpBase {
    static constexpr auto kOpcodeType = IROpcodeType::Subtract;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }
};

// Reverse subtract
//   rsb[s]   <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <rhs> - <lhs> and stores the result in <dst>.
// Updates host flags if [s] is specified.
struct IRReverseSubtractOp : public detail::IRBinaryOpBase {
    static constexpr auto kOpcodeType = IROpcodeType::ReverseSubtract;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }
};

// Add
//   add[s]   <var?:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> + <rhs> and stores the result in <dst> if present.
// The CMN operation omits <dst>.
// Updates host flags if [s] is specified.
struct IRAddOp : public detail::IRComparisonOpBase {
    static constexpr auto kOpcodeType = IROpcodeType::Add;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }
};

// Add with carry
//   adc[s]   <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> + <rhs> + (carry) and stores the result in <dst>.
// Updates host flags if [s] is specified.
struct IRAddCarryOp : public detail::IRBinaryOpBase {
    static constexpr auto kOpcodeType = IROpcodeType::AddCarry;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }
};

// Subtract with carry
//   sbc[s]   <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> - <rhs> - (carry) and stores the result in <dst>.
// Updates host flags if [s] is specified.
struct IRSubtractCarryOp : public detail::IRBinaryOpBase {
    static constexpr auto kOpcodeType = IROpcodeType::SubtractCarry;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }
};

// Reverse subtract with carry
//   rsc[s]   <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <rhs> - <lhs> - (carry) and stores the result in <dst>.
// Updates host flags if [s] is specified.
struct IRReverseSubtractCarryOp : public detail::IRBinaryOpBase {
    static constexpr auto kOpcodeType = IROpcodeType::ReverseSubtractCarry;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }
};

// Bitwise OR
//   orr[s]   <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> OR <rhs> and stores the result in <dst>.
// Updates host flags if [s] is specified.
struct IRBitwiseOrOp : public detail::IRBinaryOpBase {
    static constexpr auto kOpcodeType = IROpcodeType::BitwiseOr;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }
};

// Move
//   mov[s]   <var:dst>, <var/imm:value>
//
// Copies <value> into <dst>.
// Updates host flags if [s] is specified.
struct IRMoveOp : public detail::IRUnaryOpBase {
    static constexpr auto kOpcodeType = IROpcodeType::Move;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }
};

// Bit clear
//   bic[s]   <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Clears the bits set in <rhs> from <lhs> and stores the result into <dst>.
// Updates host flags if [s] is specified.
struct IRBitClearOp : public detail::IRBinaryOpBase {
    static constexpr auto kOpcodeType = IROpcodeType::BitClear;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }
};

// Move negated
//   mvn[s]   <var:dst>, <var/imm:value>
//
// Copies <value> negated into <dst>.
// Updates host flags if [s] is specified.
struct IRMoveNegatedOp : public detail::IRUnaryOpBase {
    static constexpr auto kOpcodeType = IROpcodeType::MoveNegated;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }
};

// Count leading zeros
//   clz   <var:dst>, <var/imm:value>
//
// Counts 0 bits from the least significant bit until the first 1 in <value> and stores the result in <dst>.
// Stores 32 if <value> is zero.
struct IRCountLeadingZerosOp : public IROpBase {
    static constexpr auto kOpcodeType = IROpcodeType::CountLeadingZeros;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }

    VariableArg dst;
    VarOrImmArg value;
};

// Saturating add
//   q[d]add  <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> + <rhs> (signed) with saturation and stores the result in <dst>.
// <rhs> is doubled before the addition if [d] is specified.
// Updates the Q host flag if the doubling operation or the addition saturates.
struct IRSaturatingAddOp : public detail::IRSaturatingBinaryOpBase {
    static constexpr auto kOpcodeType = IROpcodeType::SaturatingAdd;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }
};

// Saturating subtract
//   q[d]sub  <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> - <rhs> (signed) with saturation and stores the result in <dst>.
// <rhs> is doubled before the subtraction if [d] is specified.
// Updates the Q host flag if the doubling operation or the subtraction saturates.
struct IRSaturatingSubtractOp : public detail::IRSaturatingBinaryOpBase {
    static constexpr auto kOpcodeType = IROpcodeType::SaturatingSubtract;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }
};

// Multiply
//   mul[s]   <var:dstLo>, <var?:dstHi>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> * <rhs> and stores the least significant word of the result in <dstLo>.
// Stores the most significant word of the result in <dstHi> if present.
// Updates host flags is [s] is specified.
struct IRMultiplyOp : public IROpBase {
    static constexpr auto kOpcodeType = IROpcodeType::Multiply;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }

    VariableArg dstLo;
    std::optional<VariableArg> dstHi;
    VarOrImmArg lhs;
    VarOrImmArg rhs;
    bool setFlags;
};

// Add long
//   addl[s] <var:dstLo>, <var:dstHi>, <var/imm:lhsLo>, <var/imm:lhsHi>, <var/imm:rhsLo>, <var/imm:rhsHi>
//
// Adds the 64 bit values <lhsLo>:<lhsHi> + <rhsLo>:<rhsHi> and stores the result in <dstLo>:<dstHi>.
// Updates host flags if [s] is specified.
struct IRAddLongOp : public IROpBase {
    static constexpr auto kOpcodeType = IROpcodeType::AddLong;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }

    VariableArg dstLo;
    VariableArg dstHi;
    VarOrImmArg lhsLo;
    VarOrImmArg lhsHi;
    VarOrImmArg rhsLo;
    VarOrImmArg rhsHi;
    bool setFlags;
};

} // namespace armajitto::ir
