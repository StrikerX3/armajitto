#pragma once

#include "armajitto/ir/defs/opcode_types.hpp"

namespace armajitto::ir {

struct IROp {};

// Base type for all IR opcodes.
template <IROpcodeType opcodeType>
struct IROpBase : public IROp {
    IROpcodeType GetType() const {
        return opcodeType
    }
};

} // namespace armajitto::ir
