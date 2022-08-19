#pragma once

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
    BasicBlock(LocationRef location)
        : m_location(location) {}

    LocationRef Location() const {
        return m_location;
    }

    arm::Condition Condition() const {
        return m_cond;
    }

    uint32_t InstructionCount() const {
        return m_instrCount;
    }

    const std::vector<std::unique_ptr<IROp>> &Ops() const {
        return m_ops;
    }

    class Writer {
    public:
        Writer(BasicBlock &block)
            : m_block(block)
            , m_insertionPoint(block.m_ops.end()) {}

        BasicBlock &Block() {
            return m_block;
        }

        void NextInstruction() {
            ++m_block.m_instrCount;
        }

        void SetCondition(arm::Condition cond) {
            m_block.m_cond = cond;
        }

        size_t GetCodeSize() const {
            return m_block.m_ops.size();
        }

        size_t GetCursorPos() const {
            return std::distance(m_block.m_ops.begin(), m_insertionPoint);
        }

        bool IsCursorAtEnd() const {
            return m_insertionPoint == m_block.m_ops.end();
        }

        void SetCursorPos(size_t index) {
            assert(index <= m_block.m_ops.size());
            m_insertionPoint = m_block.m_ops.begin() + index;
        }

        size_t MoveCursor(int64_t offset) {
            assert(static_cast<size_t>(GetCursorPos() + offset) <= m_block.m_ops.size());
            m_insertionPoint += offset;
            return GetCursorPos();
        }

        IROp &GetOp(size_t index) {
            assert(index < m_block.m_ops.size());
            return *m_block.m_ops.at(index);
        }

        IROp *GetCurrentOp() {
            if (m_insertionPoint != m_block.m_ops.end()) {
                return m_insertionPoint->get();
            } else {
                return nullptr;
            }
        }

        void OverwriteNext() {
            m_overwriteNext = true;
        }

        void Erase(size_t pos, size_t count) {
            assert(count >= 1);
            auto it = m_block.m_ops.begin() + pos;
            m_block.m_ops.erase(it, it + count);
        }

        void EraseNext(size_t count) {
            assert(count >= 1);
            m_block.m_ops.erase(m_insertionPoint, m_insertionPoint + count);
        }

        template <typename T, typename... Args>
        void InsertOp(Args &&...args) {
            if (m_overwriteNext) {
                *m_insertionPoint = std::make_unique<T>(std::forward<Args>(args)...);
                m_overwriteNext = false;
            } else {
                m_insertionPoint =
                    std::next(m_block.m_ops.insert(m_insertionPoint, std::make_unique<T>(std::forward<Args>(args)...)));
            }
        }

    private:
        BasicBlock &m_block;
        std::vector<std::unique_ptr<IROp>>::iterator m_insertionPoint;
        bool m_overwriteNext = false;
    };

private:
    LocationRef m_location;
    arm::Condition m_cond;

    std::vector<std::unique_ptr<IROp>> m_ops;
    uint32_t m_instrCount = 0; // ARM/Thumb instructions
};

} // namespace armajitto::ir
