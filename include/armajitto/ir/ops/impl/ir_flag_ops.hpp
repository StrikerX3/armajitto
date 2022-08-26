#pragma once

#include "armajitto/guest/arm/flags.hpp"
#include "armajitto/ir/defs/arguments.hpp"
#include "armajitto/ir/ops/ir_ops_base.hpp"

#include <format>

namespace armajitto::ir {

// Store flags
//   stflg.[n][z][c][v] <var/imm:values>
//
// Sets the host flags specified by [n][z][c][v] to <values>.
// The position of the bits in <values> must match those in CPSR -- bit 31 is N, bit 30 is Z, and so on.
struct IRStoreFlagsOp : public IROpBase<IROpcodeType::StoreFlags> {
    arm::Flags flags;
    VarOrImmArg values;

    IRStoreFlagsOp(arm::Flags flags, VarOrImmArg values)
        : flags(flags)
        , values(values) {}

    std::string ToString() const final {
        auto flagsSuffix = arm::FlagsSuffixStr(flags, flags);
        if (values.immediate) {
            auto flagsVal = static_cast<arm::Flags>(values.imm.value);
            auto flagsStr = arm::FlagsStr(flagsVal, flagsVal);
            return std::format("stflg{} {{{}}}", flagsSuffix, flagsStr);
        } else {
            return std::format("stflg{} {}", flagsSuffix, values.var.ToString());
        }
    }
};

// Load flags
//   ldflg.[n][z][c][v] <var:dst_cpsr>, <var:src_cpsr>
//
// Load the specified [n][z][c][v] flags in <src_cpsr> from the host's flags and stores the result in <dst_cpsr>.
struct IRLoadFlagsOp : public IROpBase<IROpcodeType::LoadFlags> {
    arm::Flags flags;
    VariableArg dstCPSR;
    VarOrImmArg srcCPSR;

    IRLoadFlagsOp(arm::Flags flags, VariableArg dstCPSR, VarOrImmArg srcCPSR)
        : flags(flags)
        , dstCPSR(dstCPSR)
        , srcCPSR(srcCPSR) {}

    std::string ToString() const final {
        auto flagsSuffix = arm::FlagsSuffixStr(flags, flags);
        return std::format("ldflg{} {}, {}", flagsSuffix, dstCPSR.ToString(), srcCPSR.ToString());
    }
};

// Load sticky overflow flag
//   ldflg.q <var:dst_cpsr>, <var:src_cpsr>
//
// Loads the Q flag in <src_cpsr> if the host overflow flag is set and stores the result in <dst_cpsr>.
struct IRLoadStickyOverflowOp : public IROpBase<IROpcodeType::LoadStickyOverflow> {
    bool setQ;
    VariableArg dstCPSR;
    VarOrImmArg srcCPSR;

    IRLoadStickyOverflowOp(VariableArg dstCPSR, VarOrImmArg srcCPSR)
        : setQ(true)
        , dstCPSR(dstCPSR)
        , srcCPSR(srcCPSR) {}

    std::string ToString() const final {
        return std::format("ldflg{} {}, {}", (setQ ? ".q" : ""), dstCPSR.ToString(), srcCPSR.ToString());
    }
};

} // namespace armajitto::ir
