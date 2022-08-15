#pragma once

#include "armajitto/ir/defs/opcode_types.hpp"

namespace armajitto::ir {

// Base type for all IR opcodes.
struct IROpBase {
    virtual IROpcodeType GetOpcodeType() const = 0;
};

} // namespace armajitto::ir
