#pragma once

#include "../ir_ops_base.hpp"

#include "ir/defs/arguments.hpp"

#include <string>

namespace armajitto::ir {

// Get general purpose register value
//   ld <var:dst>, <gpr:src>
//
// Copies the value of the <src> GPR into <dst>.
struct IRGetRegisterOp : public IROpBase<IROpcodeType::GetRegister> {
    VariableArg dst;
    GPRArg src;

    IRGetRegisterOp(VariableArg dst, GPRArg src)
        : dst(dst)
        , src(src) {}

    std::string ToString() const final {
        return std::string("ld ") + dst.ToString() + ", " + src.ToString();
    }
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

    std::string ToString() const final {
        return std::string("st ") + dst.ToString() + ", " + src.ToString();
    }
};

// Get CPSR value
//   ld <var:dst>, cpsr
//
// Copies the value of CPSR into <dst>.
struct IRGetCPSROp : public IROpBase<IROpcodeType::GetCPSR> {
    VariableArg dst;

    IRGetCPSROp(VariableArg dst)
        : dst(dst) {}

    std::string ToString() const final {
        return std::string("ld ") + dst.ToString() + ", cpsr";
    }
};

// Set CPSR value
//   st cpsr[.c], <var/imm:src>
//
// Copies the value of <src> into CPSR.
// Also updates the host I flag and copies CPSR to SPSR if [c] is specified.
// The SPSR copy only happens if the target mode has a banked SPSR.
struct IRSetCPSROp : public IROpBase<IROpcodeType::SetCPSR> {
    VarOrImmArg src;
    bool updateSPSRAndIFlag;

    IRSetCPSROp(VarOrImmArg src, bool updateSPSRAndIFlag)
        : src(src)
        , updateSPSRAndIFlag(updateSPSRAndIFlag) {}

    std::string ToString() const final {
        return std::string(updateSPSRAndIFlag ? "st cpsr.c" : "st cpsr") + ", " + src.ToString();
    }
};

// Get SPSR value
//   ld <var:dst>, spsr_<mode>
//
// Copies the value of the specified <mode>'s SPSR into <dst>.
struct IRGetSPSROp : public IROpBase<IROpcodeType::GetSPSR> {
    VariableArg dst;
    arm::Mode mode;

    IRGetSPSROp(VariableArg dst, arm::Mode mode)
        : dst(dst)
        , mode(mode) {}

    std::string ToString() const final {
        return std::string("ld ") + dst.ToString() + ", spsr_" + ::armajitto::arm::ToString(mode);
    }
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

    std::string ToString() const final {
        return std::string("st spsr_") + ::armajitto::arm::ToString(mode) + ", " + src.ToString();
    }
};

} // namespace armajitto::ir
