#pragma once

#include "armajitto/guest/arm/flags.hpp"
#include "armajitto/ir/defs/arguments.hpp"
#include "ir_ops_base.hpp"

namespace armajitto::ir {

// Store flags
//   sflg.[n][z][c][v][q] <var:dst_cpsr>, <var:src_cpsr>, <var/imm:values>
//
// Sets the flags specified in the mask [n][z][c][v][q] in <src_cpsr> to the corresponding bits in <values> and stores
// the result in <dst_cpsr>.
// The flag bit locations in <values> match those in CPSR.
struct IRStoreFlagsOp : public IROpBase<IROpcodeType::StoreFlags> {
    Flags flags;
    VariableArg dstCPSR;
    VariableArg srcCPSR;

    IRStoreFlagsOp(Flags flags, VariableArg dstCPSR, VariableArg srcCPSR)
        : flags(flags)
        , dstCPSR(dstCPSR)
        , srcCPSR(srcCPSR) {}
};

// Update flags
//   uflg.[n][z][c][v]    <var:dst_cpsr>, <var:src_cpsr>
//
// Updates the specified [n][z][c][v] flags in <src_cpsr> using the host's flags and stores the result in <dst_cpsr>.
struct IRUpdateFlagsOp : public IROpBase<IROpcodeType::UpdateFlags> {
    Flags flags;
    VariableArg dstCPSR;
    VariableArg srcCPSR;

    IRUpdateFlagsOp(Flags flags, VariableArg dstCPSR, VariableArg srcCPSR)
        : flags(flags)
        , dstCPSR(dstCPSR)
        , srcCPSR(srcCPSR) {}
};

// UpdateStickyOverflow
//   uflg.q      <var:dst_cpsr>, <var:src_cpsr>
//
// Sets the Q flag in <src_cpsr> if the host overflow flag is set and stores the result in <dst_cpsr>.
struct IRUpdateStickyOverflowOp : public IROpBase<IROpcodeType::UpdateStickyOverflow> {
    VariableArg dstCPSR;
    VariableArg srcCPSR;

    IRUpdateStickyOverflowOp(VariableArg dstCPSR, VariableArg srcCPSR)
        : dstCPSR(dstCPSR)
        , srcCPSR(srcCPSR) {}
};

} // namespace armajitto::ir
