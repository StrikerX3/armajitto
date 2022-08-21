#pragma once

#include "armajitto/guest/arm/flags.hpp"
#include "armajitto/ir/defs/arguments.hpp"
#include "armajitto/ir/ops/ir_ops_base.hpp"

#include <format>

namespace armajitto::ir {

// Store flags
//   stflg.[n][z][c][v][q] <var:dst_cpsr>, <var:src_cpsr>, <var/imm:values>
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
        return std::format("stflg{} {}, {}, {}", flagsSuffix, dstCPSR.ToString(), srcCPSR.ToString(),
                           values.ToString());
    }
};

// Load flags
//   ldflg.[n][z][c][v] <var:dst_cpsr>, <var:src_cpsr>
//
// Load the specified [n][z][c][v] flags in <src_cpsr> from the host's flags and stores the result in <dst_cpsr>.
struct IRLoadFlagsOp : public IROpBase<IROpcodeType::LoadFlags> {
    arm::Flags flags;
    VariableArg dstCPSR;
    VariableArg srcCPSR;

    IRLoadFlagsOp(arm::Flags flags, VariableArg dstCPSR, VariableArg srcCPSR)
        : flags(flags & ~arm::Flags::Q)
        , dstCPSR(dstCPSR)
        , srcCPSR(srcCPSR) {}

    std::string ToString() const final {
        auto flagsSuffix = arm::FlagsSuffixStr(flags);
        return std::format("ldflg{} {}, {}", flagsSuffix, dstCPSR.ToString(), srcCPSR.ToString());
    }
};

// Load sticky overflow flag
//   ldflg.q <var:dst_cpsr>, <var:src_cpsr>
//
// Load the Q flag in <src_cpsr> if the host overflow flag is set and stores the result in <dst_cpsr>.
struct IRLoadStickyOverflowOp : public IROpBase<IROpcodeType::LoadStickyOverflow> {
    VariableArg dstCPSR;
    VariableArg srcCPSR;

    IRLoadStickyOverflowOp(VariableArg dstCPSR, VariableArg srcCPSR)
        : dstCPSR(dstCPSR)
        , srcCPSR(srcCPSR) {}

    std::string ToString() const final {
        return std::format("ldflg.q {}, {}", dstCPSR.ToString(), srcCPSR.ToString());
    }
};

} // namespace armajitto::ir
