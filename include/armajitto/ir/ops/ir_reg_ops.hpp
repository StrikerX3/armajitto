#pragma once

#include "armajitto/ir/defs/arg_refs.hpp"
#include "ir_ops_base.hpp"

namespace armajitto::ir {

// Get general purpose register value
//   ld <gpr:src>, <var:dst>
//
// Copies the value of the <src> GPR into <dst>.
struct IRGetRegisterOp : public IROpBase<IROpcodeType::GetRegister> {
    VariableArg dst;
    GPRArg src;

    IRGetRegisterOp(VariableArg dst, GPRArg src)
        : dst(dst)
        , src(src) {}
};

// Set general purpose register value
//   st <gpr:dst>, <var/imm:src>
//
// Copies the value of <src> into the <dst> GPR.
struct IRSetRegisterOp : public IROpBase<IROpcodeType::SetRegister> {
    GPRArg dst;
    VarOrImmArg src;

    IRSetRegisterOp(GPRArg dst, VarOrImmArg src)
        : dst(dst)
        , src(src) {}
};

// Get CPSR value
//   ld cpsr, <var:dst>
//
// Copies the value of CPSR into <dst>.
struct IRGetCPSROp : public IROpBase<IROpcodeType::GetCPSR> {
    VariableArg dst;

    IRGetCPSROp(VariableArg dst)
        : dst(dst) {}
};

// Set CPSR value
//   st cpsr, <var/imm:src>
//
// Copies the value of <src> into CPSR.
struct IRSetCPSROp : public IROpBase<IROpcodeType::SetCPSR> {
    VarOrImmArg src;

    IRSetCPSROp(VarOrImmArg src)
        : src(src) {}
};

// Get SPSR value
//   ld spsr_<mode>, <var:dst>
//
// Copies the value of the specified <mode>'s SPSR into <dst>.
struct IRGetSPSROp : public IROpBase<IROpcodeType::GetSPSR> {
    arm::Mode mode;
    VariableArg dst;

    IRGetSPSROp(arm::Mode mode, VariableArg dst)
        : mode(mode)
        , dst(dst) {}
};

// Set SPSR value
//   st spsr_<mode>, <var/imm:src>
//
// Copies the value of <src> into the specified <mode>'s SPSR.
struct IRSetSPSROp : public IROpBase<IROpcodeType::SetSPSR> {
    arm::Mode mode;
    VarOrImmArg src;

    IRSetSPSROp(arm::Mode mode, VarOrImmArg src)
        : mode(mode)
        , src(src) {}
};

} // namespace armajitto::ir
