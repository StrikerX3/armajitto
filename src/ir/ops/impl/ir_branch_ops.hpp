#pragma once

#include "../ir_ops_base.hpp"

#include "ir/defs/arguments.hpp"

#include <string>

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
        return std::string("b ") + address.ToString();
    }
};

// Branch and exchange
//   bx[4/t] <var/imm:address>
//
// Performs a branch to <address>, switching ARM/Thumb state based on the specified mode.
// The address is aligned to a word or halfword boundary, depending on the specified ARM/Thumb state.
// This instruction reads CPSR and modifies PC and CPSR and should be the last instruction in a block.
// If [4] is specified, the exchange will only happen if the CP15 L4 bit (ARMv5 branch and exchange backwards
// compatibility) is clear.
// If [t] is specified, the exchange happens based on the current CPSR T bit.
// If neither [4] nor [t] is specified, the mode is set based on bit 0 of the address.
struct IRBranchExchangeOp : public IROpBase<IROpcodeType::BranchExchange> {
    enum class ExchangeMode { AddrBit0, L4, CPSRThumbFlag };
    ExchangeMode bxMode;
    VarOrImmArg address;

    IRBranchExchangeOp(VarOrImmArg address)
        : bxMode(ExchangeMode::AddrBit0)
        , address(address) {}

    IRBranchExchangeOp(VarOrImmArg address, ExchangeMode bxMode)
        : bxMode(bxMode)
        , address(address) {}

    std::string ToString() const final {
        std::string mnemonic;
        switch (bxMode) {
        case ExchangeMode::AddrBit0: mnemonic = "bx"; break;
        case ExchangeMode::L4: mnemonic = "bx4"; break;
        case ExchangeMode::CPSRThumbFlag: mnemonic = "bxt"; break;
        default: mnemonic = "bx?"; break;
        }
        return mnemonic + " " + address.ToString();
    }
};

} // namespace armajitto::ir
