#pragma once

#include "armajitto/guest/arm/flags.hpp"
#include "armajitto/ir/defs/arguments.hpp"
#include "armajitto/ir/ops/ir_ops_base.hpp"

#include <format>

namespace armajitto::ir {

namespace detail {
    // Base type of bit shifting ALU operations.
    //   [op].[c] <var:dst>, <var/imm:value>, <var/imm:amount>
    template <IROpcodeType opcodeType>
    struct IRShiftOpBase : public IROpBase<opcodeType> {
        VariableArg dst;
        VarOrImmArg value;
        VarOrImmArg amount;
        bool setCarry;

        IRShiftOpBase(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setCarry, const char *mnemonic)
            : dst(dst)
            , value(value)
            , amount(amount)
            , setCarry(setCarry)
            , mnemonic(mnemonic) {}

        std::string ToString() const final {
            return std::format("{}{} {}, {}, {}", mnemonic, (setCarry ? ".c" : ""), dst.ToString(), value.ToString(),
                               amount.ToString());
        }

    private:
        const char *mnemonic;
    };

    // Base type of binary ALU operations, including comparison instructions.
    //   [op].[n][z][c][v] <var:dst>, <var/imm:lhs>, <var/imm:rhs>
    //   [op].[n][z][c][v] <var/imm:lhs>, <var/imm:rhs>
    template <IROpcodeType opcodeType, arm::Flags affectedFlags>
    struct IRBinaryOpBase : public IROpBase<opcodeType> {
        VariableArg dst;
        VarOrImmArg lhs;
        VarOrImmArg rhs;
        arm::Flags flags;

        static constexpr arm::Flags kAffectedFlags = affectedFlags;

        IRBinaryOpBase(VarOrImmArg lhs, VarOrImmArg rhs, arm::Flags flags, const char *mnemonic)
            : lhs(lhs)
            , rhs(rhs)
            , flags(flags)
            , mnemonic(mnemonic) {}

        IRBinaryOpBase(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, arm::Flags flags, const char *mnemonic)
            : dst(dst)
            , lhs(lhs)
            , rhs(rhs)
            , flags(flags)
            , mnemonic(mnemonic) {}

        std::string ToString() const final {
            auto flagsSuffix = arm::FlagsSuffixStr(flags, affectedFlags);
            if (dst.var.IsPresent()) {
                return std::format("{}{} {}, {}, {}", mnemonic, flagsSuffix, dst.ToString(), lhs.ToString(),
                                   rhs.ToString());
            } else {
                return std::format("{}{} {}, {}", mnemonic, flagsSuffix, lhs.ToString(), rhs.ToString());
            }
        }

    private:
        const char *mnemonic;
    };

    // Base type of unary ALU operations.
    //   [op].[n][z] <var:dst>, <var/imm:value>
    template <IROpcodeType opcodeType>
    struct IRUnaryOpBase : public IROpBase<opcodeType> {
        VariableArg dst;
        VarOrImmArg value;
        arm::Flags flags;

        static constexpr arm::Flags kAffectedFlags = arm::Flags::NZ;

        IRUnaryOpBase(VariableArg dst, VarOrImmArg value, arm::Flags flags, const char *mnemonic)
            : dst(dst)
            , value(value)
            , flags(flags)
            , mnemonic(mnemonic) {}

        std::string ToString() const final {
            auto flagsSuffix = arm::FlagsSuffixStr(flags, kAffectedFlags);
            return std::format("{}{} {}, {}", mnemonic, flagsSuffix, dst.ToString(), value.ToString());
        }

    private:
        const char *mnemonic;
    };
} // namespace detail

// -----------------------------------------------------------------------------

// Logical shift left
//   lsl.[c] <var:dst>, <var/imm:value>, <var/imm:amount>
//
// Shifts bits in <value> left by <amount>, shifting in zeros, and stores the result in <dst>.
// Updates host carry flags if [c] is specified.
struct IRLogicalShiftLeftOp : public detail::IRShiftOpBase<IROpcodeType::LogicalShiftLeft> {
    IRLogicalShiftLeftOp(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setCarry)
        : IRShiftOpBase(dst, value, amount, setCarry, "lsl") {}
};

// Logical shift right
//   lsr.[c] <var:dst>, <var/imm:value>, <var/imm:amount>
//
// Shifts bits in value <right> by <amount>, shifting in zeros, and stores the result in <dst>.
// Updates host carry flag if [c] is specified.
struct IRLogicalShiftRightOp : public detail::IRShiftOpBase<IROpcodeType::LogicalShiftRight> {
    IRLogicalShiftRightOp(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setCarry)
        : IRShiftOpBase(dst, value, amount, setCarry, "lsr") {}
};

// Arithmetic shift right
//   asr.[c] <var:dst>, <var/imm:value>, <var/imm:amount>
//
// Shifts bits in <value> right by <amount>, shifting in the sign bit of <value>, and stores the result in <dst>.
// Updates host carry flag if [c] is specified.
struct IRArithmeticShiftRightOp : public detail::IRShiftOpBase<IROpcodeType::ArithmeticShiftRight> {
    IRArithmeticShiftRightOp(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setCarry)
        : IRShiftOpBase(dst, value, amount, setCarry, "asr") {}
};

// Rotate right
//   ror.[c] <var:dst>, <var/imm:value>, <var/imm:amount>
//
// Rotates bits in <value> right by <amount> and stores the result in <dst>.
// Updates host carry flag if [c] is specified.
struct IRRotateRightOp : public detail::IRShiftOpBase<IROpcodeType::RotateRight> {
    IRRotateRightOp(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setCarry)
        : IRShiftOpBase(dst, value, amount, setCarry, "ror") {}
};

// Rotate right extended
//   rrx.[c] <var:dst>, <var/imm:value>
//
// Rotates bits in <value> right by one, shifting in the carry flag, and stores the result in <dst>.
// Updates host carry flag if [c] is specified.
struct IRRotateRightExtendedOp : public IROpBase<IROpcodeType::RotateRightExtended> {
    VariableArg dst;
    VarOrImmArg value;
    bool setCarry;

    IRRotateRightExtendedOp(VariableArg dst, VarOrImmArg value, bool setCarry)
        : dst(dst)
        , value(value)
        , setCarry(setCarry) {}

    std::string ToString() const final {
        return std::format("rrx{} {}, {}", (setCarry ? ".c" : ""), dst.ToString(), value.ToString());
    }
};

// Bitwise AND
//   and.[n][z] <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//   tst <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> AND <rhs> and stores the result in <dst> if present.
// The TST operation omits <dst>.
// Updates the host flags specified by [n][z]. TST always updates flags.
struct IRBitwiseAndOp : public detail::IRBinaryOpBase<IROpcodeType::BitwiseAnd, arm::Flags::NZ> {
    // Constructor for the TST operation.
    IRBitwiseAndOp(VarOrImmArg lhs, VarOrImmArg rhs)
        : IRBinaryOpBase(lhs, rhs, arm::Flags::NZ, "tst") {}

    // Constructor for the AND operation.
    IRBitwiseAndOp(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags)
        : IRBinaryOpBase(dst, lhs, rhs, (setFlags ? arm::Flags::NZ : arm::Flags::None), "and") {}
};

// Bitwise OR
//   orr.[n][z] <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> OR <rhs> and stores the result in <dst>.
// Updates the host flags specified by [n][z].
struct IRBitwiseOrOp : public detail::IRBinaryOpBase<IROpcodeType::BitwiseOr, arm::Flags::NZ> {
    IRBitwiseOrOp(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags)
        : IRBinaryOpBase(dst, lhs, rhs, (setFlags ? arm::Flags::NZ : arm::Flags::None), "orr") {}
};

// Bitwise XOR
//   eor.[n][z] <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//   teq <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> XOR <rhs> and stores the result in <dst> if present.
// The TEQ operation omits <dst>.
// Updates the host flags specified by [n][z]. TEQ always updates flags.
struct IRBitwiseXorOp : public detail::IRBinaryOpBase<IROpcodeType::BitwiseXor, arm::Flags::NZ> {
    // Constructor for the TEQ operation.
    IRBitwiseXorOp(VarOrImmArg lhs, VarOrImmArg rhs)
        : IRBinaryOpBase(lhs, rhs, arm::Flags::NZ, "teq") {}

    // Constructor for the EOR operation.
    IRBitwiseXorOp(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags)
        : IRBinaryOpBase(dst, lhs, rhs, (setFlags ? arm::Flags::NZ : arm::Flags::None), "eor") {}
};

// Bit clear
//   bic.[n][z] <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Clears the bits set in <rhs> from <lhs> and stores the result into <dst>.
// Updates the host flags specified by [n][z].
struct IRBitClearOp : public detail::IRBinaryOpBase<IROpcodeType::BitClear, arm::Flags::NZ> {
    IRBitClearOp(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags)
        : IRBinaryOpBase(dst, lhs, rhs, (setFlags ? arm::Flags::NZ : arm::Flags::None), "bic") {}
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
//   add.[n][z][c][v] <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//   cmn <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> + <rhs> and stores the result in <dst> if present.
// The CMN operation omits <dst>.
// Updates the host flags specified by [n][z][c][v]. CMN always updates flags.
struct IRAddOp : public detail::IRBinaryOpBase<IROpcodeType::Add, arm::Flags::NZCV> {
    // Constructor for the CMN operation.
    IRAddOp(VarOrImmArg lhs, VarOrImmArg rhs)
        : IRBinaryOpBase(lhs, rhs, arm::Flags::NZCV, "cmn") {}

    // Constructor for the ADD operation.
    IRAddOp(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags)
        : IRBinaryOpBase(dst, lhs, rhs, (setFlags ? arm::Flags::NZCV : arm::Flags::None), "add") {}
};

// Add with carry
//   adc.[n][z][c][v] <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> + <rhs> + (carry) and stores the result in <dst>.
// Updates the host flags specified by [n][z][c][v].
struct IRAddCarryOp : public detail::IRBinaryOpBase<IROpcodeType::AddCarry, arm::Flags::NZCV> {
    IRAddCarryOp(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags)
        : IRBinaryOpBase(dst, lhs, rhs, (setFlags ? arm::Flags::NZCV : arm::Flags::None), "adc") {}
};

// Subtract
//   sub.[n][z][c][v] <var?:dst>, <var/imm:lhs>, <var/imm:rhs>
//   cmp <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> - <rhs> and stores the result in <dst> if present.
// The CMP operation omits <dst>.
// Updates the host flags specified by [n][z][c][v]. CMP always updates flags.
struct IRSubtractOp : public detail::IRBinaryOpBase<IROpcodeType::Subtract, arm::Flags::NZCV> {
    // Constructor for the CMP operation.
    IRSubtractOp(VarOrImmArg lhs, VarOrImmArg rhs)
        : IRBinaryOpBase(lhs, rhs, arm::Flags::NZCV, "cmp") {}

    // Constructor for the SUB operation.
    IRSubtractOp(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags)
        : IRBinaryOpBase(dst, lhs, rhs, (setFlags ? arm::Flags::NZCV : arm::Flags::None), "sub") {}
};

// Subtract with carry
//   sbc.[n][z][c][v] <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> - <rhs> - (1 - carry) and stores the result in <dst>.
// Updates the host flags specified by [n][z][c][v].
struct IRSubtractCarryOp : public detail::IRBinaryOpBase<IROpcodeType::SubtractCarry, arm::Flags::NZCV> {
    IRSubtractCarryOp(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags)
        : IRBinaryOpBase(dst, lhs, rhs, (setFlags ? arm::Flags::NZCV : arm::Flags::None), "sbc") {}
};

// Move
//   mov.[n][z] <var:dst>, <var/imm:value>
//
// Copies <value> into <dst>.
// Updates the host flags specified by [n][z].
struct IRMoveOp : public detail::IRUnaryOpBase<IROpcodeType::Move> {
    IRMoveOp(VariableArg dst, VarOrImmArg value, bool setFlags)
        : IRUnaryOpBase(dst, value, (setFlags ? arm::Flags::NZ : arm::Flags::None), "mov") {}
};

// Move negated
//   mvn.[n][z] <var:dst>, <var/imm:value>
//
// Copies <value> negated into <dst>.
// Updates the host flags specified by [n][z].
struct IRMoveNegatedOp : public detail::IRUnaryOpBase<IROpcodeType::MoveNegated> {
    IRMoveNegatedOp(VariableArg dst, VarOrImmArg value, bool setFlags)
        : IRUnaryOpBase(dst, value, (setFlags ? arm::Flags::NZ : arm::Flags::None), "mvn") {}
};

// Saturating add
//   qadd.[v] <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> + <rhs> (signed) with saturation and stores the result in <dst>.
// Updates the V host flag if the addition saturates and [v] is specified.
struct IRSaturatingAddOp : public detail::IRBinaryOpBase<IROpcodeType::SaturatingAdd, arm::Flags::V> {
    IRSaturatingAddOp(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setQ)
        : IRBinaryOpBase(dst, lhs, rhs, (setQ ? arm::Flags::V : arm::Flags::None), "qadd") {}
};

// Saturating subtract
//   qsub.[v] <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> - <rhs> (signed) with saturation and stores the result in <dst>.
// Updates the V host flag if the subtraction saturates and [v] is specified.
struct IRSaturatingSubtractOp : public detail::IRBinaryOpBase<IROpcodeType::SaturatingSubtract, arm::Flags::V> {
    IRSaturatingSubtractOp(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setQ)
        : IRBinaryOpBase(dst, lhs, rhs, (setQ ? arm::Flags::V : arm::Flags::None), "qsub") {}
};

// Multiply
//   [u/s]mul.[n][z] <var:dst>, <var/imm:lhs>, <var/imm:rhs>
//
// Computes <lhs> * <rhs> and stores the result in <dst>.
// [u/s] specifies if the multiplication is [u]nsigned or [s]igned.
// Updates the host flags specified by [n][z].
struct IRMultiplyOp : public IROpBase<IROpcodeType::Multiply> {
    VariableArg dst;
    VarOrImmArg lhs;
    VarOrImmArg rhs;
    bool signedMul;
    arm::Flags flags;

    static constexpr arm::Flags kAffectedFlags = arm::Flags::NZ;

    IRMultiplyOp(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool signedMul, bool setFlags)
        : dst(dst)
        , lhs(lhs)
        , rhs(rhs)
        , signedMul(signedMul)
        , flags(setFlags ? arm::Flags::NZ : arm::Flags::None) {}

    std::string ToString() const final {
        auto flagsSuffix = arm::FlagsSuffixStr(flags, kAffectedFlags);
        return std::format("{}mul{} {}, {}, {}", (signedMul ? "s" : "u"), flagsSuffix, dst.ToString(), lhs.ToString(),
                           rhs.ToString());
    }
};

// Multiply long
//   [u/s]mull[h].[n][z] <var:dstHi>:<var:dstLo>, <var/imm:lhs>:<var/imm:rhs>
//
// Computes <lhs> * <rhs> and stores the least significant word of the result in <dstLo> and the most significant word
// in <dstHi>.
// The result is shifted right by 16 bits (a halfword) if [h] is specified.
// [u/s] specifies if the multiplication is [u]nsigned or [s]igned.
// Updates the host flags specified by [n][z].
struct IRMultiplyLongOp : public IROpBase<IROpcodeType::MultiplyLong> {
    VariableArg dstLo;
    VariableArg dstHi;
    VarOrImmArg lhs;
    VarOrImmArg rhs;
    bool signedMul;
    bool shiftDownHalf;
    arm::Flags flags;

    static constexpr arm::Flags kAffectedFlags = arm::Flags::NZ;

    IRMultiplyLongOp(VariableArg dstLo, VariableArg dstHi, VarOrImmArg lhs, VarOrImmArg rhs, bool signedMul,
                     bool shiftDownHalf, bool setFlags)
        : dstLo(dstLo)
        , dstHi(dstHi)
        , lhs(lhs)
        , rhs(rhs)
        , signedMul(signedMul)
        , shiftDownHalf(shiftDownHalf)
        , flags(setFlags ? arm::Flags::NZ : arm::Flags::None) {}

    std::string ToString() const final {
        auto flagsSuffix = arm::FlagsSuffixStr(flags, kAffectedFlags);
        return std::format("{}mull{}{} {}:{}, {}, {}", (signedMul ? "s" : "u"), (shiftDownHalf ? "h" : ""), flagsSuffix,
                           dstHi.ToString(), dstLo.ToString(), lhs.ToString(), rhs.ToString());
    }
};

// Add long
//   addl.[n][z] <var:dstHi>:<var:dstLo>, <var/imm:lhsHi>:<var/immLohsHi>, <var/imm:rhsHi>:<var/immLohsHi>
//
// Adds the 64-bit values <lhsHi>:<lhsLo> + <rhsHi>:<rhsLo> and stores the result in <dstHi>:<dstLo>.
// Updates the host flags specified by [n][z].
struct IRAddLongOp : public IROpBase<IROpcodeType::AddLong> {
    VariableArg dstLo;
    VariableArg dstHi;
    VarOrImmArg lhsLo;
    VarOrImmArg lhsHi;
    VarOrImmArg rhsLo;
    VarOrImmArg rhsHi;
    arm::Flags flags;

    static constexpr arm::Flags kAffectedFlags = arm::Flags::NZ;

    IRAddLongOp(VariableArg dstLo, VariableArg dstHi, VarOrImmArg lhsLo, VarOrImmArg lhsHi, VarOrImmArg rhsLo,
                VarOrImmArg rhsHi, bool setFlags)
        : dstLo(dstLo)
        , dstHi(dstHi)
        , lhsLo(lhsLo)
        , lhsHi(lhsHi)
        , rhsLo(rhsLo)
        , rhsHi(rhsHi)
        , flags(setFlags ? arm::Flags::NZ : arm::Flags::None) {}

    std::string ToString() const final {
        auto flagsSuffix = arm::FlagsSuffixStr(flags, kAffectedFlags);
        return std::format("addl{} {}:{}, {}:{}, {}:{}", flagsSuffix, dstHi.ToString(), dstLo.ToString(),
                           lhsHi.ToString(), lhsLo.ToString(), rhsHi.ToString(), rhsLo.ToString());
    }
};

} // namespace armajitto::ir
