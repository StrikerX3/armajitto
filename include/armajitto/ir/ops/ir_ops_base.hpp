#pragma once

#include "armajitto/ir/defs/opcode_types.hpp"

#include <optional>
#include <string>

namespace armajitto::ir {

struct IROp {
    virtual ~IROp() = default;

    virtual IROpcodeType GetType() const = 0;
    virtual std::string ToString() const = 0;
};

// Base type for all IR opcodes.
template <IROpcodeType opcodeType>
struct IROpBase : public IROp {
    static constexpr IROpcodeType kOpcodeType = opcodeType;

    IROpcodeType GetType() const final {
        return opcodeType;
    }
};

template <typename T>
std::optional<T &> Cast(IROp &op) {
    if (T::kOpcodeType == op.GetType()) {
        return static_cast<T &>(op);
    } else {
        return std::nullopt;
    }
}

} // namespace armajitto::ir
