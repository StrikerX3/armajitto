#pragma once

#include "armajitto/ir/defs/opcode_types.hpp"

#include <optional>
#include <string>

namespace armajitto::ir {

struct IROp {
    virtual IROpcodeType GetType() const = 0;
    virtual std::string ToString() const = 0;

    IROp *Prev() {
        return prev;
    }

    IROp *Next() {
        return next;
    }

    const IROp *Prev() const {
        return prev;
    }

    const IROp *Next() const {
        return next;
    }

private:
    void Append(IROp *op) {
        op->next = next;
        op->prev = this;
        if (next != nullptr) {
            next->prev = op;
        }
        next = op;
    }

    void Prepend(IROp *op) {
        op->prev = prev;
        op->next = this;
        if (prev != nullptr) {
            prev->next = op;
        }
        prev = op;
    }

    void Replace(IROp *op) {
        op->prev = prev;
        op->next = next;
        if (next != nullptr) {
            next->prev = op;
        }
        if (prev != nullptr) {
            prev->next = op;
        }
        next = prev = nullptr;
    }

    IROp *Erase() {
        IROp *result = next;
        if (prev != nullptr) {
            prev->next = next;
        }
        if (next != nullptr) {
            next->prev = prev;
        }
        prev = nullptr;
        next = nullptr;
        return result;
    }

    IROp *prev = nullptr;
    IROp *next = nullptr;

    friend class BasicBlock;
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
std::optional<T *> Cast(IROp *op) {
    if (op == nullptr) {
        return std::nullopt;
    } else if (T::kOpcodeType == op->GetType()) {
        return static_cast<T *>(op);
    } else {
        return std::nullopt;
    }
}

} // namespace armajitto::ir
