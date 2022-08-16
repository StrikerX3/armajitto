#pragma once

#include "armajitto/ir/defs/arg_refs.hpp"
#include "armajitto/ir/defs/memory_access.hpp"
#include "basic_block.hpp"

#include <vector>

namespace armajitto::ir {

class Emitter {
public:
    Emitter(BasicBlock &block)
        : m_block(block)
        , m_insertionPoint(block.m_ops.end()) {}

    Variable Var(const char *name);

    BasicBlock &GetBlock() {
        return m_block;
    }

    void NextInstruction(arm::Condition cond);

    void LoadGPR(VariableArg dst, GPRArg src);
    void StoreGPR(GPRArg dst, VarOrImmArg src);
    void LoadCPSR(VariableArg dst);
    void StoreCPSR(VarOrImmArg src);
    void LoadSPSR(arm::Mode mode, VariableArg dst);
    void StoreSPSR(arm::Mode mode, VarOrImmArg src);

    void MemRead(MemAccessMode mode, MemAccessSize size, VariableArg dst, VarOrImmArg address);
    void MemWrite(MemAccessSize size, VarOrImmArg src, VarOrImmArg address);

    void CountLeadingZeros(VariableArg dst, VarOrImmArg value);

    void InstructionFetch();

private:
    BasicBlock &m_block;

    using OpIterator = std::vector<IROp *>::iterator;

    OpIterator m_insertionPoint;

    template <typename T, typename... Args>
    OpIterator InsertOp(OpIterator insertionPoint, Args &&...args) {
        return m_block.m_ops.insert(insertionPoint, new T(std::forward<Args>(args)...));
    }

    template <typename T, typename... Args>
    void PrependOp(Args &&...args) {
        m_insertionPoint = InsertOp<T, Args...>(m_insertionPoint, std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    void AppendOp(Args &&...args) {
        m_insertionPoint = std::next(InsertOp<T, Args...>(m_insertionPoint, std::forward<Args>(args)...));
    }
};

} // namespace armajitto::ir
