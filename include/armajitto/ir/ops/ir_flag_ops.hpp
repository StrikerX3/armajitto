#pragma once

#include "armajitto/ir/defs/arg_refs.hpp"
#include "ir_ops_base.hpp"

namespace armajitto::ir {

// Store flags
//   sflg.[n][z][c][v][q] <var:dst_cpsr>, <var:src_cpsr>, <var/imm:values>
//
// Sets the flags specified in the mask [n][z][c][v][q] in <src_cpsr> to the corresponding bits in <values> and stores
// the result in <dst_cpsr>.
// The flag bit locations in <values> match those in CPSR.
struct IRStoreFlagsOp : public IROpBase {
    static constexpr auto kOpcodeType = IROpcodeType::StoreFlags;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }

    uint8_t mask;
    VariableArg dstCPSR;
    VariableArg srcCPSR;
};

// Update flags
//   uflg.[n][z][c][v]    <var:dst_cpsr>, <var:src_cpsr>
//
// Updates the specified [n][z][c][v] flags in <src_cpsr> using the host's flags and stores the result in <dst_cpsr>.
struct IRUpdateFlagsOp : public IROpBase {
    static constexpr auto kOpcodeType = IROpcodeType::UpdateFlags;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }

    uint8_t mask;
    VariableArg dstCPSR;
    VariableArg srcCPSR;
};
// UpdateStickyOverflow
//   uflg.q      <var:dst_cpsr>, <var:src_cpsr>
//
// Sets the Q flag in <src_cpsr> if the host overflow flag is set and stores the result in <dst_cpsr>.
struct IRUpdateStickyOverflowOp : public IROpBase {
    static constexpr auto kOpcodeType = IROpcodeType::UpdateStickyOverflow;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }

    VariableArg dstCPSR;
    VariableArg srcCPSR;
};

} // namespace armajitto::ir
