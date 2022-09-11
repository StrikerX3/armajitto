#pragma once

#include "../ir_ops_base.hpp"

#include "ir/defs/arguments.hpp"

#include <format>

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
        return std::format("ld {}, {}", dst.ToString(), src.ToString());
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
        return std::format("st {}, {}", dst.ToString(), src.ToString());
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
        return std::format("ld {}, cpsr", dst.ToString());
    }
};

// Set CPSR value
//   st cpsr[.i], <var/imm:src>
//
// Copies the value of <src> into CPSR.
// Also updates the host I flag if [i] is specified.
struct IRSetCPSROp : public IROpBase<IROpcodeType::SetCPSR> {
    VarOrImmArg src;
    bool updateIFlag;

    IRSetCPSROp(VarOrImmArg src, bool updateIFlag)
        : src(src)
        , updateIFlag(updateIFlag) {}

    std::string ToString() const final {
        return std::format("st cpsr{}, {}", (updateIFlag ? ".i" : ""), src.ToString());
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
        return std::format("ld {}, spsr_{}", dst.ToString(), ::armajitto::arm::ToString(mode));
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
        return std::format("st spsr_{}, {}", ::armajitto::arm::ToString(mode), src.ToString());
    }
};

} // namespace armajitto::ir
