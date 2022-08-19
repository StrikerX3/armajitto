#pragma once

#include "armajitto/guest/arm/flags.hpp"
#include "armajitto/ir/defs/arguments.hpp"
#include "ir_ops_base.hpp"

#include <format>

namespace armajitto::ir {

// Store flags
//   sflg.[n][z][c][v][q] <var:dst_cpsr>, <var:src_cpsr>, <var/imm:values>
//
// Copies the flags specified in the mask [n][z][c][v][q] from <values> into <src_cpsr> and stores the result in
// <dst_cpsr>.
// The position of the bits in <values> must match those in CPSR -- bit 31 is N, bit 30 is Z, and so on.
// The host flags are also updated to the specified values.
struct IRStoreFlagsOp : public IROpBase<IROpcodeType::StoreFlags> {
    Flags flags;
    VariableArg dstCPSR;
    VariableArg srcCPSR;
    VarOrImmArg values;

    IRStoreFlagsOp(Flags flags, VariableArg dstCPSR, VariableArg srcCPSR, VarOrImmArg values)
        : flags(flags)
        , dstCPSR(dstCPSR)
        , srcCPSR(srcCPSR)
        , values(values) {}

    std::string ToString() const final {
        auto flg = [](bool value, const char *letter) { return value ? std::string(letter) : std::string(""); };
        auto bmFlags = BitmaskEnum(flags);
        auto n = flg(bmFlags.AnyOf(Flags::N), "n");
        auto z = flg(bmFlags.AnyOf(Flags::Z), "z");
        auto c = flg(bmFlags.AnyOf(Flags::C), "c");
        auto v = flg(bmFlags.AnyOf(Flags::V), "v");
        auto q = flg(bmFlags.AnyOf(Flags::Q), "q");
        return std::format("sflg.{}{}{}{}{} {}, {}, {}", n, z, c, v, q, dstCPSR.ToString(), srcCPSR.ToString(),
                           values.ToString());
    }
};

// Update flags
//   uflg.[n][z][c][v] <var:dst_cpsr>, <var:src_cpsr>
//
// Updates the specified [n][z][c][v] flags in <src_cpsr> using the host's flags and stores the result in <dst_cpsr>.
struct IRUpdateFlagsOp : public IROpBase<IROpcodeType::UpdateFlags> {
    Flags flags;
    VariableArg dstCPSR;
    VariableArg srcCPSR;

    IRUpdateFlagsOp(Flags flags, VariableArg dstCPSR, VariableArg srcCPSR)
        : flags(flags & ~Flags::Q)
        , dstCPSR(dstCPSR)
        , srcCPSR(srcCPSR) {}

    std::string ToString() const final {
        auto flg = [](bool value, const char *letter) { return value ? std::string(letter) : std::string(""); };
        auto bmFlags = BitmaskEnum(flags);
        auto n = flg(bmFlags.AnyOf(Flags::N), "n");
        auto z = flg(bmFlags.AnyOf(Flags::Z), "z");
        auto c = flg(bmFlags.AnyOf(Flags::C), "c");
        auto v = flg(bmFlags.AnyOf(Flags::V), "v");
        return std::format("uflg.{}{}{}{} {}, {}", n, z, c, v, dstCPSR.ToString(), srcCPSR.ToString());
    }
};

// UpdateStickyOverflow
//   uflg.q <var:dst_cpsr>, <var:src_cpsr>
//
// Sets the Q flag in <src_cpsr> if the host overflow flag is set and stores the result in <dst_cpsr>.
struct IRUpdateStickyOverflowOp : public IROpBase<IROpcodeType::UpdateStickyOverflow> {
    VariableArg dstCPSR;
    VariableArg srcCPSR;

    IRUpdateStickyOverflowOp(VariableArg dstCPSR, VariableArg srcCPSR)
        : dstCPSR(dstCPSR)
        , srcCPSR(srcCPSR) {}

    std::string ToString() const final {
        return std::format("uflg.q {}, {}", dstCPSR.ToString(), srcCPSR.ToString());
    }
};

} // namespace armajitto::ir
