#pragma once

#include "ir_ops_base.hpp"

namespace armajitto::ir {

// Load GPR
//   ld <gpr:src>, <var:dst>
//
// Copies the value of the <src> GPR into <dst>.
struct IRLoadGPROp : public IROpBase {
    static constexpr auto opcodeType = IROpcodeType::LoadGPR;

    IROpcodeType GetOpcodeType() const final {
        return opcodeType;
    }

    GPRArg src;
    VariableArg dst;
};

// Store GPR
//   st <gpr:dst>, <var/imm:src>
//
// Copies the value of <src> into the <dst> GPR.
struct IRStoreGPROp : public IROpBase {
    static constexpr auto opcodeType = IROpcodeType::StoreGPR;

    IROpcodeType GetOpcodeType() const final {
        return opcodeType;
    }

    VarOrImmArg src;
    GPRArg dst;
};

// Load PSR
//   ld <psr:src>, <var:dst>
//
// Copies the value of the <src> PSR into <dst>.
struct IRLoadPSROp : public IROpBase {
    static constexpr auto opcodeType = IROpcodeType::LoadPSR;

    IROpcodeType GetOpcodeType() const final {
        return opcodeType;
    }

    PSRArg src;
    VariableArg dst;
};

// Store PSR
//   st <psr:dst>, <var/imm:src>
//
// Copies the value of <src> into the <dst> PSR.
struct IRStorePSROp : public IROpBase {
    static constexpr auto opcodeType = IROpcodeType::StorePSR;

    IROpcodeType GetOpcodeType() const final {
        return opcodeType;
    }

    VarOrImmArg src;
    PSRArg dst;
};

} // namespace armajitto::ir
