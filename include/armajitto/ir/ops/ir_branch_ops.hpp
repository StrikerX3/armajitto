#pragma once

#include "armajitto/ir/defs/arg_refs.hpp"
#include "ir_ops_base.hpp"

namespace armajitto::ir {

// Branch
//   b  <var:dst_pc>, <var/imm:src_cpsr>, <var/imm:address>
//
// Branches to <address> using the current ARM/Thumb mode in <src_cpsr> and stores the resulting PC in <dst_pc>.
struct IRBranchOp : public IROpBase<IROpcodeType::Branch> {
    VariableArg dstPC;
    VarOrImmArg srcCPSR;
    VarOrImmArg address;

    IRBranchOp(VariableArg dstPC, VarOrImmArg srcCPSR, VarOrImmArg address)
        : dstPC(dstPC)
        , srcCPSR(srcCPSR)
        , address(address) {}
};

// Branch with exchange
//   bx <var:dst_pc>, <var/imm:dst_cpsr>, <var/imm:src_cpsr>, <var/imm:address>
//
// Branches to <address>, switching to ARM/Thumb mode based on bit 0 of <address>, and stores the resulting PC in
// <dst_pc>. The CPSR T flag is updated from <src_cpsr> to <dst_cpsr>.
struct IRBranchExchangeOp : public IROpBase<IROpcodeType::BranchExchange> {
    VariableArg dstPC;
    VariableArg dstCPSR;
    VarOrImmArg srcCPSR;
    VarOrImmArg address;

    IRBranchExchangeOp(VariableArg dstPC, VariableArg dstCPSR, VarOrImmArg srcCPSR, VarOrImmArg address)
        : dstPC(dstPC)
        , dstCPSR(dstCPSR)
        , srcCPSR(srcCPSR)
        , address(address) {}
};

} // namespace armajitto::ir
