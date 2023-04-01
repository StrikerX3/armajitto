#pragma once

#include "../ir_ops_base.hpp"

#include "ir/defs/arguments.hpp"
#include "ir/defs/memory_access.hpp"

#include <string>

namespace armajitto::ir {

// Memory read
//   ld.[c/d][a/u/s][b/h/w] <var:dst>, [<var/imm:address>]
// where:
//   [c/d]   = code/data (bus)
//   [b/h/w] = byte/half/word (size)
//   [a/u/s] = aligned/unaligned/signed (mode)
//             a is hidden
//             s sign-extends, a and u zero-extend
//   Valid size/mode combinations: (a)b, (a)h, (a)w, uh, uw, sb, sh
//   Code reads can only be aligned halfword or word
//
// Reads a byte, halfword or word from address into the dst variable.
// Byte and halfword reads extend values to 32 bits.
// Signed reads use sign-extension. Other reads use zero-extension.
// Unaligned halfword and word reads may force-align or rotate the word, depending on the CPU architecture.
struct IRMemReadOp : public IROpBase<IROpcodeType::MemRead> {
    MemAccessBus bus;
    MemAccessMode mode;
    MemAccessSize size;
    VariableArg dst;
    VarOrImmArg address;

    IRMemReadOp(MemAccessBus bus, MemAccessMode mode, MemAccessSize size, VariableArg dst, VarOrImmArg address)
        : bus(bus)
        , mode(mode)
        , size(size)
        , dst(dst)
        , address(address) {}

    std::string ToString() const final {
        char busStr;
        switch (bus) {
        case MemAccessBus::Code: busStr = 'c'; break;
        case MemAccessBus::Data: busStr = 'd'; break;
        default: busStr = '?'; break;
        }

        const char *modeStr;
        switch (mode) {
        case MemAccessMode::Aligned: modeStr = ""; break;
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

        return std::string("ld.") + busStr + modeStr + sizeStr + " " + dst.ToString() + ", [" + address.ToString() +
               "]";
    }
};

// Memory write
//   st.[b/h/w] <var/imm:src>, [<var/imm:address>]
// where:
//   [b/h/w] = byte/half/word
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

        return std::string("st.") + sizeStr + " " + src.ToString() + ", [" + address.ToString() + "]";
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
        return std::string("pld [") + address.ToString() + "]";
    }
};

} // namespace armajitto::ir
