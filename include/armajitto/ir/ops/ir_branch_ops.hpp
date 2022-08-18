#pragma once

#include "armajitto/ir/defs/arguments.hpp"
#include "ir_ops_base.hpp"

#include <format>

namespace armajitto::ir {

// Branch
//   b <var:dst_pc>, <var/imm:src_cpsr>, <var/imm:address>
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

    std::string ToString() const final {
        return std::format("b {}, {}, {}", dstPC.ToString(), srcCPSR.ToString(), address.ToString());
    }
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

    std::string ToString() const final {
        return std::format("bx {}, {}, {}, {}", dstPC.ToString(), dstCPSR.ToString(), srcCPSR.ToString(),
                           address.ToString());
    }
};

} // namespace armajitto::ir
