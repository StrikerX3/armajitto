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

    void NextInstruction();
    void SetCondition(arm::Condition cond);

    void GetRegister(VariableArg dst, GPRArg src);
    void SetRegister(GPRArg dst, VarOrImmArg src);
    void GetCPSR(VariableArg dst);
    void SetCPSR(VarOrImmArg src);
    void GetSPSR(arm::Mode mode, VariableArg dst);
    void SetSPSR(arm::Mode mode, VarOrImmArg src);

    void MemRead(MemAccessMode mode, MemAccessSize size, VariableArg dst, VarOrImmArg address);
    void MemWrite(MemAccessSize size, VarOrImmArg src, VarOrImmArg address);

    void LogicalShiftLeft(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setFlags);
    void LogicalShiftRight(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setFlags);
    void ArithmeticShiftRight(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setFlags);
    void RotateRight(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setFlags);
    void RotateRightExtend(VariableArg dst, VarOrImmArg value, bool setFlags);
    void BitwiseAnd(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    void BitwiseXor(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    void Subtract(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    void ReverseSubtract(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    void Add(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    void AddCarry(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    void SubtractCarry(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    void ReverseSubtractCarry(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    void BitwiseOr(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    void Move(VariableArg dst, VarOrImmArg value, bool setFlags);
    void BitClear(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    void MoveNegated(VariableArg dst, VarOrImmArg value, bool setFlags);
    void CountLeadingZeros(VariableArg dst, VarOrImmArg value);
    void SaturatingAdd(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs);
    void SaturatingSubtract(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs);
    void Multiply(VariableArg dstLo, VariableArg dstHi, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    void AddLong(VariableArg dstLo, VariableArg dstHi, VarOrImmArg lhsLo, VarOrImmArg lhsHi, VarOrImmArg rhsLo,
                 VarOrImmArg rhsHi, bool setFlags);

    void StoreFlags(uint8_t mask, VariableArg dstCPSR, VariableArg srcCPSR);
    void UpdateFlags(uint8_t mask, VariableArg dstCPSR, VariableArg srcCPSR);
    void UpdateStickyOverflow(VariableArg dstCPSR, VariableArg srcCPSR);

    void Branch(VariableArg dstPC, VarOrImmArg srcCPSR, VarOrImmArg address);
    void BranchExchange(VariableArg dstPC, VariableArg dstCPSR, VarOrImmArg srcCPSR, VarOrImmArg address);

    void LoadCopRegister(VariableArg dstValue, uint8_t cpnum, uint8_t opcode1, uint8_t crn, uint8_t crm,
                         uint8_t opcode2, bool ext);
    void StoreCopRegister(VarOrImmArg srcValue, uint8_t cpnum, uint8_t opcode1, uint8_t crn, uint8_t crm,
                          uint8_t opcode2, bool ext);

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
