#pragma once

#include "armajitto/ir/defs/arg_refs.hpp"
#include "armajitto/ir/defs/memory_access.hpp"
#include "ir_ops_base.hpp"

namespace armajitto::ir {

// [b/h/w] = byte/half/word
// [r/s/u] = raw/signed/unaligned
//           r is hidden
//           s sign-extends, r and u zero-extend
// Valid combinations: (r)b, (r)h, (r)w, sb, sh, uh, uw

// Memory read
//   ld.[r/s/u][b/h/w] <var:dst>, [<any:address>]
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
};

// Memory write
//   st.[b/h/w]        <any:src>, [<any:address>]
//
// Writes a byte, halfword or word from src into memory at address.
struct IRMemWriteOp : public IROpBase<IROpcodeType::MemWrite> {
    MemAccessSize size;
    VarOrImmArg src;
    VarOrImmArg address;
};

} // namespace armajitto::ir
