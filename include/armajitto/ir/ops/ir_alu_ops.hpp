#pragma once

#include "armajitto/ir/defs/arguments.hpp"
#include "ir_ops_base.hpp"

#include <format>

namespace armajitto::ir {

// [s] = updates host flags

namespace detail {
    // Base type of bit shifting ALU operations.
    //   [op][s] <var:dst>, <var/imm:value>, <var/imm:amount>
    template <IROpcodeType opcodeType>
    struct IRShiftOpBase : public IROpBase<opcodeType> {
        VariableArg dst;
        VarOrImmArg value;
        VarOrImmArg amount;
        bool setFlags;

        IRShiftOpBase(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setFlags, const char *mnemonic)
            : dst(dst)
            , value(value)
            , amount(amount)
            , setFlags(setFlags)
            , mnemonic(mnemonic) {}

        std::string ToString() const final {
            return std::format("{}{} {}, {}, {}", mnemonic, (setFlags ? "s" : ""), dst.ToString(), value.ToString(),
                               amount.ToString());
        }

    private:
        const char *mnemonic;
    };

    // Base type of binary ALU operations, including comparison instructions.
    //   [op][s] <var:dst>, <var/imm:lhs>, <var/imm:rhs>
    //   [op][s] <var?:dst>, <var/imm:lhs>, <var/imm:rhs>
    template <IROpcodeType opcodeType>
    struct IRBinaryOpBase : public IROpBase<opcodeType> {
        VariableArg dst;
        VarOrImmArg lhs;
        VarOrImmArg rhs;
        bool setFlags;

        IRBinaryOpBase(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags, const char *mnemonic)
            : lhs(lhs)
            , rhs(rhs)
            , setFlags(setFlags)
            , mnemonic(mnemonic) {}

        IRBinaryOpBase(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags, const char *mnemonic)
            : dst(dst)
            , lhs(lhs)
            , rhs(rhs)
            , setFlags(setFlags)
            , mnemonic(mnemonic) {}

        std::string ToString() const final {
            if (dst.var.IsPresent()) {
                return std::format("{}{} {}, {}, {}", mnemonic, (setFlags ? "s" : ""), dst.ToString(), lhs.ToString(),
                                   rhs.ToString());
            } else {
                return std::format("{}{} {}, {}", mnemonic, (setFlags ? "s" : ""), lhs.ToString(), rhs.ToString());
            }
        }

    private:
        const char *mnemonic;
    };

    // Base type of saturating binary ALU operations.
    //   [op] <var:dst>, <var/imm:lhs>, <var/imm:rhs>
    template <IROpcodeType opcodeType>
    struct IRSaturatingBinaryOpBase : public IROpBase<opcodeType> {
        VariableArg dst;
        VarOrImmArg lhs;
        VarOrImmArg rhs;

        IRSaturatingBinaryOpBase(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, const char *mnemonic)
            : dst(dst)
            , lhs(lhs)
            , rhs(rhs)
            , mnemonic(mnemonic) {}

        std::string ToString() const final {
            return std::format("{} {}, {}, {}", mnemonic, dst.ToString(), lhs.ToString(), rhs.ToString());
        }

    private:
        const char *mnemonic;
    };

    // Base type of unary ALU operations.
    //   [op][s] <var:dst>, <var/imm:value>
    template <IROpcodeType opcodeType>
    struct IRUnaryOpBase : public IROpBase<opcodeType> {
        VariableArg dst;
        VarOrImmArg value;
        bool setFlags;

        IRUnaryOpBase(VariableArg dst, VarOrImmArg value, bool setFlags, const char *mnemonic)
            : dst(dst)
            , value(value)
            , setFlags(setFlags)
            , mnemonic(mnemonic) {}

        std::string ToString() const final {
            return std::format("{}{} {}, {}", mnemonic, (setFlags ? "s" : ""), dst.ToString(), value.ToString());
        }

    private:
        const char *mnemonic;
    };
} // namespace detail

// -----------------------------------------------------------------------------

// Logical shift left
//   lsl[s] <var:dst>, <var/imm:value>, <var/imm:amount>
//
// Shifts bits in <value> left by <amount>, shifting in zeros, and stores the result in <dst>.
// Updates host flags if [s] is specified.
struct IRLogicalShiftLeftOp : public detail::IRShiftOpBase<IROpcodeType::LogicalShiftLeft> {
    IRLogicalShiftLeftOp(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setFlags)
        : IRShiftOpBase(dst, value, amount, setFlags, "lsl") {}
};

// Logical shift right
//   lsr[s] <var:dst>, <var/imm:value>, <var/imm:amount>
//
// Shifts bits in value <right> by <amount>, shifting in zeros, and stores the result in <dst>.
// Updates host flags if [s] is specified.
struct IRLogicalShiftRightOp : public detail::IRShiftOpBase<IROpcodeType::LogicalShiftRight> {
    IRLogicalShiftRightOp(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setFlags)
        : IRShiftOpBase(dst, value, amount, setFlags, "lsr") {}
};

// Arithmetic shift right
//   asr[s] <var:dst>, <var/imm:value>, <var/imm:amount>
//
// Shifts bits in <value> right by <amount>, shifting in the sign bit of <value>, and stores the result in <dst>.
// Updates host flags if [s] is specified.
struct IRArithmeticShiftRightOp : public detail::IRShiftOpBase<IROpcodeType::ArithmeticShiftRight> {
    IRArithmeticShiftRightOp(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setFlags)
        : IRShiftOpBase(dst, value, amount, setFlags, "asr") {}
};

// Rotate right
//   ror[s] <var:dst>, <var/imm:value>, <var/imm:amount>
//
// Rotates bits in <value> right by <amount> and stores the result in <dst>.
// Updates host flags if [s] is specified.
struct IRRotateRightOp : public detail::IRShiftOpBase<IROpcodeType::RotateRight> {
    IRRotateRightOp(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setFlags)
        : IRShiftOpBase(dst, value, amount, setFlags, "ror") {}
};

// Rotate right extend
//   rrx[s] <var:dst>, <var/imm:value>
//
// Rotates bits in <value> right by one, shifting in the carry flag, and stores the result in <dst>.
// Updates host flags if [s] is specified.
struct IRRotateRightExtendOp : public IROpBase<IROpcodeType::RotateRightExtend> {
    VariableArg dst;
    VarOrImmArg value;
    bool setFlags;

    IRRotateRightExtendOp(VariableArg dst, VarOrImmArg value, bool setFlags)
        : dst(dst)
        , value(value)
        , setFlags(setFlags) {}

    std::string ToString() const final {
        return std::format("rrx{} {}, {}", (setFlags ? "s" : ""), dst.ToString(), value.ToString());
    }
};

// Bitwise AND
//   and[s] <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//   tst <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> AND <rhs> and stores the result in <dst> if present.
// The TST operation omits <dst>.
// Updates host flags if [s] is specified. TST always updates flags.
struct IRBitwiseAndOp : public detail::IRBinaryOpBase<IROpcodeType::BitwiseAnd> {
    // Constructor for the TST operation.
    IRBitwiseAndOp(VarOrImmArg lhs, VarOrImmArg rhs)
        : IRBinaryOpBase(lhs, rhs, true, "tst") {}

    // Constructor for the AND operation.
    IRBitwiseAndOp(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags)
        : IRBinaryOpBase(dst, lhs, rhs, setFlags, "and") {}
};

// Bitwise OR
//   orr[s] <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> OR <rhs> and stores the result in <dst>.
// Updates host flags if [s] is specified.
struct IRBitwiseOrOp : public detail::IRBinaryOpBase<IROpcodeType::BitwiseOr> {
    IRBitwiseOrOp(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags)
        : IRBinaryOpBase(dst, lhs, rhs, setFlags, "orr") {}
};

// Bitwise XOR
//   eor[s] <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//   teq <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> XOR <rhs> and stores the result in <dst> if present.
// The TEQ operation omits <dst>.
// Updates host flags if [s] is specified. TEQ always updates flags.
struct IRBitwiseXorOp : public detail::IRBinaryOpBase<IROpcodeType::BitwiseXor> {
    // Constructor for the TEQ operation.
    IRBitwiseXorOp(VarOrImmArg lhs, VarOrImmArg rhs)
        : IRBinaryOpBase(lhs, rhs, true, "teq") {}

    // Constructor for the EOR operation.
    IRBitwiseXorOp(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags)
        : IRBinaryOpBase(dst, lhs, rhs, setFlags, "eor") {}
};

// Bit clear
//   bic[s] <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Clears the bits set in <rhs> from <lhs> and stores the result into <dst>.
// Updates host flags if [s] is specified.
struct IRBitClearOp : public detail::IRBinaryOpBase<IROpcodeType::BitClear> {
    IRBitClearOp(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags)
        : IRBinaryOpBase(dst, lhs, rhs, setFlags, "bic") {}
};

// Count leading zeros
//   clz <var:dst>, <var/imm:value>
//
// Counts 0 bits from the least significant bit until the first 1 in <value> and stores the result in <dst>.
// Stores 32 if <value> is zero.
struct IRCountLeadingZerosOp : public IROpBase<IROpcodeType::CountLeadingZeros> {
    VariableArg dst;
    VarOrImmArg value;

    IRCountLeadingZerosOp(VariableArg dst, VarOrImmArg value)
        : dst(dst)
        , value(value) {}

    std::string ToString() const final {
        return std::format("clz {}, {}", dst.ToString(), value.ToString());
    }
};

// Add
//   add[s] <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//   cmn <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> + <rhs> and stores the result in <dst> if present.
// The CMN operation omits <dst>.
// Updates host flags if [s] is specified. CMN always updates flags.
struct IRAddOp : public detail::IRBinaryOpBase<IROpcodeType::Add> {
    // Constructor for the CMN operation.
    IRAddOp(VarOrImmArg lhs, VarOrImmArg rhs)
        : IRBinaryOpBase(lhs, rhs, true, "cmn") {}

    // Constructor for the ADD operation.
    IRAddOp(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags)
        : IRBinaryOpBase(dst, lhs, rhs, setFlags, "add") {}
};

// Add with carry
//   adc[s] <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> + <rhs> + (carry) and stores the result in <dst>.
// Updates host flags if [s] is specified.
struct IRAddCarryOp : public detail::IRBinaryOpBase<IROpcodeType::AddCarry> {
    IRAddCarryOp(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags)
        : IRBinaryOpBase(dst, lhs, rhs, setFlags, "adc") {}
};

// Subtract
//   sub[s] <var?:dst>, <var/imm:lhs>, <var/imm:rhs>
//   cmp <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> - <rhs> and stores the result in <dst> if present.
// The CMP operation omits <dst>.
// Updates host flags if [s] is specified. CMP always updates flags.
struct IRSubtractOp : public detail::IRBinaryOpBase<IROpcodeType::Subtract> {
    // Constructor for the CMP operation.
    IRSubtractOp(VarOrImmArg lhs, VarOrImmArg rhs)
        : IRBinaryOpBase(lhs, rhs, true, "cmp") {}

    // Constructor for the SUB operation.
    IRSubtractOp(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags)
        : IRBinaryOpBase(dst, lhs, rhs, setFlags, "sub") {}
};

// Subtract with carry
//   sbc[s] <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> - <rhs> - (1 - carry) and stores the result in <dst>.
// Updates host flags if [s] is specified.
struct IRSubtractCarryOp : public detail::IRBinaryOpBase<IROpcodeType::SubtractCarry> {
    IRSubtractCarryOp(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags)
        : IRBinaryOpBase(dst, lhs, rhs, setFlags, "sbc") {}
};

// Move
//   mov[s] <var:dst>, <var/imm:value>
//
// Copies <value> into <dst>.
// Updates host flags if [s] is specified.
struct IRMoveOp : public detail::IRUnaryOpBase<IROpcodeType::Move> {
    IRMoveOp(VariableArg dst, VarOrImmArg value, bool setFlags)
        : IRUnaryOpBase(dst, value, setFlags, "mov") {}
};

// Move negated
//   mvn[s] <var:dst>, <var/imm:value>
//
// Copies <value> negated into <dst>.
// Updates host flags if [s] is specified.
struct IRMoveNegatedOp : public detail::IRUnaryOpBase<IROpcodeType::MoveNegated> {
    IRMoveNegatedOp(VariableArg dst, VarOrImmArg value, bool setFlags)
        : IRUnaryOpBase(dst, value, setFlags, "mvn") {}
};

// Saturating add
//   qadd <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> + <rhs> (signed) with saturation and stores the result in <dst>.
// Updates the Q host flag if the addition saturates.
struct IRSaturatingAddOp : public detail::IRSaturatingBinaryOpBase<IROpcodeType::SaturatingAdd> {
    IRSaturatingAddOp(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs)
        : IRSaturatingBinaryOpBase(dst, lhs, rhs, "qadd") {}
};

// Saturating subtract
//   qsub <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> - <rhs> (signed) with saturation and stores the result in <dst>.
// Updates the Q host flag if the subtraction saturates.
struct IRSaturatingSubtractOp : public detail::IRSaturatingBinaryOpBase<IROpcodeType::SaturatingSubtract> {
    IRSaturatingSubtractOp(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs)
        : IRSaturatingBinaryOpBase(dst, lhs, rhs, "qsub") {}
};

// Multiply
//   [u/s]mul[s] <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> * <rhs> and stores the result in <dst>.
// [u/s] specifies if the multiplication is [u]nsigned or [s]igned.
// Updates host flags is [s] is specified.
struct IRMultiplyOp : public IROpBase<IROpcodeType::Multiply> {
    VariableArg dst;
    VarOrImmArg lhs;
    VarOrImmArg rhs;
    bool signedMul;
    bool setFlags;

    IRMultiplyOp(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool signedMul, bool setFlags)
        : dst(dst)
        , lhs(lhs)
        , rhs(rhs)
        , signedMul(signedMul)
        , setFlags(setFlags) {}

    std::string ToString() const final {
        return std::format("{}mul{} {}, {}, {}", (signedMul ? "s" : "u"), (setFlags ? "s" : ""), dst.ToString(),
                           lhs.ToString(), rhs.ToString());
    }
};

// Multiply long
//   [u/s]mull[s][h] <var:dstLo>, <var:dstHi>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> * <rhs> and stores the least significant word of the result in <dstLo> and the most significant word
// in <dstHi>.
// The result is shifted right by 16 bits (a halfword) if [h] is specified.
// [u/s] specifies if the multiplication is [u]nsigned or [s]igned.
// Updates host flags is [s] is specified.
struct IRMultiplyLongOp : public IROpBase<IROpcodeType::MultiplyLong> {
    VariableArg dstLo;
    VariableArg dstHi;
    VarOrImmArg lhs;
    VarOrImmArg rhs;
    bool signedMul;
    bool shiftDownHalf;
    bool setFlags;

    IRMultiplyLongOp(VariableArg dstLo, VariableArg dstHi, VarOrImmArg lhs, VarOrImmArg rhs, bool signedMul,
                     bool shiftDownHalf, bool setFlags)
        : dstLo(dstLo)
        , dstHi(dstHi)
        , lhs(lhs)
        , rhs(rhs)
        , signedMul(signedMul)
        , shiftDownHalf(shiftDownHalf)
        , setFlags(setFlags) {}

    std::string ToString() const final {
        return std::format("{}mull{}{} {}, {}, {}, {}", (signedMul ? "s" : "u"), (setFlags ? "s" : ""),
                           (shiftDownHalf ? "h" : ""), dstLo.ToString(), dstHi.ToString(), lhs.ToString(),
                           rhs.ToString());
    }
};

// Add long
//   addl[s] <var:dstLo>, <var:dstHi>, <var/imm:lhsLo>, <var/imm:lhsHi>, <var/imm:rhsLo>, <var/imm:rhsHi>
//
// Adds the 64-bit values <lhsLo>:<lhsHi> + <rhsLo>:<rhsHi> and stores the result in <dstLo>:<dstHi>.
// Updates host flags if [s] is specified.
struct IRAddLongOp : public IROpBase<IROpcodeType::AddLong> {
    VariableArg dstLo;
    VariableArg dstHi;
    VarOrImmArg lhsLo;
    VarOrImmArg lhsHi;
    VarOrImmArg rhsLo;
    VarOrImmArg rhsHi;
    bool setFlags;

    IRAddLongOp(VariableArg dstLo, VariableArg dstHi, VarOrImmArg lhsLo, VarOrImmArg lhsHi, VarOrImmArg rhsLo,
                VarOrImmArg rhsHi, bool setFlags)
        : dstLo(dstLo)
        , dstHi(dstHi)
        , lhsLo(lhsLo)
        , lhsHi(lhsHi)
        , rhsLo(rhsLo)
        , rhsHi(rhsHi)
        , setFlags(setFlags) {}

    std::string ToString() const final {
        return std::format("addl{} {}, {}, {}, {}", (setFlags ? "s" : ""), dstLo.ToString(), dstHi.ToString(),
                           lhsLo.ToString(), lhsHi.ToString(), rhsLo.ToString(), rhsHi.ToString());
    }
};

} // namespace armajitto::ir
