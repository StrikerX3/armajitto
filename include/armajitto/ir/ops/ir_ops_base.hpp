#pragma once

#include "armajitto/ir/defs/opcode_types.hpp"

namespace armajitto::ir {

struct IROp {
    virtual IROpcodeType GetType() const = 0;
};

// Base type for all IR opcodes.
template <IROpcodeType opcodeType>
struct IROpBase : public IROp {
    IROpcodeType GetType() const final {
        return opcodeType;
    }
};

} // namespace armajitto::ir
