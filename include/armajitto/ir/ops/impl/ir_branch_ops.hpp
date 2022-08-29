#pragma once

#include "armajitto/ir/defs/arguments.hpp"
#include "armajitto/ir/ops/ir_ops_base.hpp"

#include <format>

namespace armajitto::ir {

// Branch
//   b <var/imm:address>
//
// Performs a branch to <address> using the current ARM/Thumb state.
// The address is aligned to a word or halfword boundary, depending on the ARM/Thumb state in CPSR.
// This instruction reads CPSR and modifies PC and should be the last instruction in a block.
struct IRBranchOp : public IROpBase<IROpcodeType::Branch> {
    VarOrImmArg address;

    IRBranchOp(VarOrImmArg address)
        : address(address) {}

    std::string ToString() const final {
        return std::format("b {}", address.ToString());
    }
};

// Branch and exchange
//   bx[4] <var/imm:address>
//
// Performs a branch to <address>, switching ARM/Thumb state based on bit 0 of the address.
// The address is aligned to a word or halfword boundary, depending on the specified ARM/Thumb state.
// This instruction reads CPSR and modifies PC and CPSR and should be the last instruction in a block.
// If [4] is specified, the exchange will only happen if the CP15 L4 bit (ARMv5 branch and exchange backwards
// compatibility) is clear.
struct IRBranchExchangeOp : public IROpBase<IROpcodeType::BranchExchange> {
    VarOrImmArg address;
    bool bx4;

    IRBranchExchangeOp(VarOrImmArg address)
        : address(address)
        , bx4(false) {}

    IRBranchExchangeOp(VarOrImmArg address, bool bx4)
        : address(address)
        , bx4(bx4) {}

    std::string ToString() const final {
        return std::format("bx{} {}", (bx4 ? "4" : ""), address.ToString());
    }
};

} // namespace armajitto::ir
