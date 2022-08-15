#pragma once

#include "armajitto/ir/defs/arg_refs.hpp"
#include "ir_ops_base.hpp"

namespace armajitto::ir {

// Load GPR
//   ld <gpr:src>, <var:dst>
//
// Copies the value of the <src> GPR into <dst>.
struct IRLoadGPROp : public IROpBase {
    static constexpr auto kOpcodeType = IROpcodeType::LoadGPR;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }

    VariableArg dst;
    GPRArg src;
};

// Store GPR
//   st <gpr:dst>, <var/imm:src>
//
// Copies the value of <src> into the <dst> GPR.
struct IRStoreGPROp : public IROpBase {
    static constexpr auto kOpcodeType = IROpcodeType::StoreGPR;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }

    GPRArg dst;
    VarOrImmArg src;
};

// Load PSR
//   ld <psr:src>, <var:dst>
//
// Copies the value of the <src> PSR into <dst>.
struct IRLoadPSROp : public IROpBase {
    static constexpr auto kOpcodeType = IROpcodeType::LoadPSR;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }

    PSRArg src;
    VariableArg dst;
};

// Store PSR
//   st <psr:dst>, <var/imm:src>
//
// Copies the value of <src> into the <dst> PSR.
struct IRStorePSROp : public IROpBase {
    static constexpr auto kOpcodeType = IROpcodeType::StorePSR;

    IROpcodeType GetOpcodeType() const final {
        return kOpcodeType;
    }

    VarOrImmArg src;
    PSRArg dst;
};

} // namespace armajitto::ir
