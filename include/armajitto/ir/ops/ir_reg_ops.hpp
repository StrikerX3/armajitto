#pragma once

#include "armajitto/ir/defs/arg_refs.hpp"
#include "ir_ops_base.hpp"

namespace armajitto::ir {

// Load GPR
//   ld <gpr:src>, <var:dst>
//
// Copies the value of the <src> GPR into <dst>.
struct IRLoadGPROp : public IROpBase<IROpcodeType::LoadGPR> {
    VariableArg dst;
    GPRArg src;

    IRLoadGPROp(VariableArg dst, GPRArg src)
        : dst(dst)
        , src(src) {}
};

// Store GPR
//   st <gpr:dst>, <var/imm:src>
//
// Copies the value of <src> into the <dst> GPR.
struct IRStoreGPROp : public IROpBase<IROpcodeType::StoreGPR> {
    GPRArg dst;
    VarOrImmArg src;

    IRStoreGPROp(GPRArg dst, VarOrImmArg src)
        : dst(dst)
        , src(src) {}
};

// Load CPSR
//   ld cpsr, <var:dst>
//
// Copies the value of CPSR into <dst>.
struct IRLoadCPSROp : public IROpBase<IROpcodeType::LoadCPSR> {
    VariableArg dst;

    IRLoadCPSROp(VariableArg dst)
        : dst(dst) {}
};

// Store CPSR
//   st cpsr, <var/imm:src>
//
// Copies the value of <src> into CPSR.
struct IRStoreCPSROp : public IROpBase<IROpcodeType::StoreCPSR> {
    VarOrImmArg src;

    IRStoreCPSROp(VarOrImmArg src)
        : src(src) {}
};

// Load SPSR
//   ld spsr_<mode>, <var:dst>
//
// Copies the value of the specified <mode>'s SPSR into <dst>.
struct IRLoadSPSROp : public IROpBase<IROpcodeType::LoadSPSR> {
    arm::Mode mode;
    VariableArg dst;

    IRLoadSPSROp(arm::Mode mode, VariableArg dst)
        : mode(mode)
        , dst(dst) {}
};

// Store SPSR
//   st spsr_<mode>, <var/imm:src>
//
// Copies the value of <src> into the specified <mode>'s SPSR.
struct IRStoreSPSROp : public IROpBase<IROpcodeType::StoreSPSR> {
    arm::Mode mode;
    VarOrImmArg src;

    IRStoreSPSROp(arm::Mode mode, VarOrImmArg src)
        : mode(mode)
        , src(src) {}
};

} // namespace armajitto::ir
