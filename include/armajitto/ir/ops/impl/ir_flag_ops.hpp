#pragma once

#include "armajitto/guest/arm/flags.hpp"
#include "armajitto/ir/defs/arguments.hpp"
#include "armajitto/ir/ops/ir_ops_base.hpp"

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
    arm::Flags flags;
    VariableArg dstCPSR;
    VariableArg srcCPSR;
    VarOrImmArg values;

    IRStoreFlagsOp(arm::Flags flags, VariableArg dstCPSR, VariableArg srcCPSR, VarOrImmArg values)
        : flags(flags)
        , dstCPSR(dstCPSR)
        , srcCPSR(srcCPSR)
        , values(values) {}

    std::string ToString() const final {
        auto flagsSuffix = arm::FlagsSuffixStr(flags);
        return std::format("sflg{} {}, {}, {}", flagsSuffix, dstCPSR.ToString(), srcCPSR.ToString(), values.ToString());
    }
};

// Update flags
//   uflg.[n][z][c][v] <var:dst_cpsr>, <var:src_cpsr>
//
// Updates the specified [n][z][c][v] flags in <src_cpsr> using the host's flags and stores the result in <dst_cpsr>.
struct IRUpdateFlagsOp : public IROpBase<IROpcodeType::UpdateFlags> {
    arm::Flags flags;
    VariableArg dstCPSR;
    VariableArg srcCPSR;

    IRUpdateFlagsOp(arm::Flags flags, VariableArg dstCPSR, VariableArg srcCPSR)
        : flags(flags & ~arm::Flags::Q)
        , dstCPSR(dstCPSR)
        , srcCPSR(srcCPSR) {}

    std::string ToString() const final {
        auto flagsSuffix = arm::FlagsSuffixStr(flags);
        return std::format("uflg{} {}, {}", flagsSuffix, dstCPSR.ToString(), srcCPSR.ToString());
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
