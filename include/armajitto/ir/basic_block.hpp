#pragma once

#include "armajitto/core/allocator.hpp"
#include "armajitto/guest/arm/instructions.hpp"
#include "armajitto/ir/ops/ir_ops_base.hpp"
#include "defs/variable.hpp"
#include "location_ref.hpp"

#include <cassert>
#include <memory>
#include <vector>

namespace armajitto::ir {

class BasicBlock {
public:
    BasicBlock(memory::Allocator &alloc, LocationRef location)
        : m_alloc(alloc)
        , m_location(location) {}

    /*~BasicBlock() {
        Clear();
    }*/

    LocationRef Location() const {
        return m_location;
    }

    arm::Condition Condition() const {
        return m_cond;
    }

    uint32_t InstructionCount() const {
        return m_instrCount;
    }

    IROp *Head() {
        return m_opsHead;
    }

    IROp *Tail() {
        return m_opsTail;
    }

    const IROp *Head() const {
        return m_opsHead;
    }

    const IROp *Tail() const {
        return m_opsTail;
    }

    void Clear() {
        IROp *op = m_opsHead;
        while (op != nullptr) {
            IROp *next = op->next;
            m_alloc.Free(op);
            op = next;
        }
        m_opsHead = nullptr;
        m_opsTail = nullptr;
    }

private:
    memory::Allocator &m_alloc;

    LocationRef m_location;
    arm::Condition m_cond;

    IROp *m_opsHead = nullptr;
    IROp *m_opsTail = nullptr;
    uint32_t m_instrCount = 0; // ARM/Thumb instructions
    uint32_t m_nextVarID = 0;

    void NextInstruction() {
        ++m_instrCount;
    }

    void SetCondition(arm::Condition cond) {
        m_cond = cond;
    }

    template <typename T, typename... Args>
    IROp *CreateOp(Args &&...args) {
        return m_alloc.Allocate<T>(std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    IROp *AppendOp(IROp *ref, Args &&...args) {
        IROp *op = CreateOp<T>(std::forward<Args>(args)...);
        if (ref == nullptr) {
            m_opsHead = m_opsTail = op;
        } else {
            ref->Append(op);
            if (ref == m_opsTail) {
                m_opsTail = op;
            }
        }
        return op;
    }

    template <typename T, typename... Args>
    IROp *PrependOp(IROp *ref, Args &&...args) {
        IROp *op = CreateOp<T>(std::forward<Args>(args)...);
        if (ref == nullptr) {
            m_opsHead = m_opsTail = op;
        } else {
            ref->Prepend(op);
            if (ref == m_opsHead) {
                m_opsHead = op;
            }
        }
        return op;
    }

    template <typename T, typename... Args>
    IROp *ReplaceOp(IROp *ref, Args &&...args) {
        IROp *op = CreateOp<T>(std::forward<Args>(args)...);
        if (ref == nullptr) {
            m_opsHead = m_opsTail = op;
        } else {
            ref->Replace(op);
            if (ref == m_opsHead) {
                m_opsHead = op;
            }
            if (ref == m_opsTail) {
                m_opsTail = op;
            }
            m_alloc.Free(ref);
        }
        return op;
    }

    void InsertHead(IROp *op) {
        if (m_opsHead == nullptr) {
            m_opsHead = op;
            m_opsTail = op;
        } else {
            m_opsHead->Prepend(op);
            m_opsHead = op;
        }
    }

    void InsertTail(IROp *op) {
        if (m_opsTail == nullptr) {
            m_opsTail = op;
            m_opsHead = op;
        } else {
            m_opsTail->Append(op);
            m_opsTail = op;
        }
    }

    void Remove(IROp *op) {
        if (op == m_opsHead) {
            m_opsHead = m_opsHead->Next();
        }
        if (op == m_opsTail) {
            m_opsTail = m_opsTail->Next();
        }
        op->Remove();
    }

    uint32_t NextVarID() {
        return m_nextVarID++;
    }

    friend class Emitter;
};

} // namespace armajitto::ir
