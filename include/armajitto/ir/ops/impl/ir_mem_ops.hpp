#pragma once

#include "armajitto/ir/defs/arguments.hpp"
#include "armajitto/ir/defs/memory_access.hpp"
#include "armajitto/ir/ops/ir_ops_base.hpp"

#include <format>

namespace armajitto::ir {

// [b/h/w] = byte/half/word
// [r/s/u] = raw/signed/unaligned
//           r is hidden
//           s sign-extends, r and u zero-extend
// Valid combinations: (r)b, (r)h, (r)w, sb, sh, uh, uw

// Memory read
//   ld.[r/s/u][b/h/w] <var:dst>, [<var/imm:address>]
//
// Reads a byte, halfword or word from address into the dst variable.
// Byte and halfword reads extend values to 32 bits.
// Signed reads use sign-extension. Other reads use zero-extension.
// Unaligned halfword and word reads may force-align or rotate the word, depending on the CPU architecture.
struct IRMemReadOp : public IROpBase<IROpcodeType::MemRead> {
    MemAccessMode mode;
    MemAccessSize size;
    VariableArg dst;
    VarOrImmArg address;

    IRMemReadOp(MemAccessMode mode, MemAccessSize size, VariableArg dst, VarOrImmArg address)
        : mode(mode)
        , size(size)
        , dst(dst)
        , address(address) {}

    std::string ToString() const final {
        const char *modeStr;
        switch (mode) {
        case MemAccessMode::Raw: modeStr = ""; break;
        case MemAccessMode::Signed: modeStr = "s"; break;
        case MemAccessMode::Unaligned: modeStr = "u"; break;
        default: modeStr = "?"; break;
        }

        char sizeStr;
        switch (size) {
        case MemAccessSize::Byte: sizeStr = 'b'; break;
        case MemAccessSize::Half: sizeStr = 'h'; break;
        case MemAccessSize::Word: sizeStr = 'w'; break;
        default: sizeStr = '?'; break;
        }

        return std::format("ld.{}{}, {}, [{}]", modeStr, sizeStr, dst.ToString(), address.ToString());
    }
};

// Memory write
//   st.[b/h/w] <var/imm:src>, [<var/imm:address>]
//
// Writes a byte, halfword or word from src into memory at address.
struct IRMemWriteOp : public IROpBase<IROpcodeType::MemWrite> {
    MemAccessSize size;
    VarOrImmArg src;
    VarOrImmArg address;

    IRMemWriteOp(MemAccessSize size, VarOrImmArg src, VarOrImmArg address)
        : size(size)
        , src(src)
        , address(address) {}

    std::string ToString() const final {
        char sizeStr;
        switch (size) {
        case MemAccessSize::Byte: sizeStr = 'b'; break;
        case MemAccessSize::Half: sizeStr = 'h'; break;
        case MemAccessSize::Word: sizeStr = 'w'; break;
        default: sizeStr = '?'; break;
        }

        return std::format("st.{}, {}, [{}]", sizeStr, src.ToString(), address.ToString());
    }
};

// Preload
//   pld [<var/imm:address>]
//
// Sends a hint to preload the specified address.
struct IRPreloadOp : public IROpBase<IROpcodeType::Preload> {
    VarOrImmArg address;

    IRPreloadOp(VarOrImmArg address)
        : address(address) {}

    std::string ToString() const final {
        return std::format("pld [{}]", address.ToString());
    }
};

} // namespace armajitto::ir