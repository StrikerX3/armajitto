#pragma once

#include "armajitto/ir/defs/arg_refs.hpp"
#include "ir_ops_base.hpp"

namespace armajitto::ir {

// Branch
//   b  <var:dst_pc>, <var/imm:src_cpsr>, <var/imm:address>
//
// Computes a branch to <address> using the current CPSR in <src_cpsr> and stores the result in <dst_pc>.
struct IRBranchOp : public IROpBase<IROpcodeType::Branch> {
    VariableArg dstPC;
    VarOrImmArg srcCPSR;
    VarOrImmArg address;

    IRBranchOp(VariableArg dstPC, VarOrImmArg srcCPSR, VarOrImmArg address)
        : dstPC(dstPC)
        , srcCPSR(srcCPSR)
        , address(address) {}
};

// Branch and exchange
//   bx <var:dst_pc>, <var:dst_cpsr>, <var/imm:src_cpsr>, <var/imm:address>
//
// Computes a branch and exchange to <address> using the current CPSR in <src_cpsr> and stores the resulting PC in
// <dst_pc> and CPSR in <dst_cpsr>.
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
