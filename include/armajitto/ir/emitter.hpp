#pragma once

#include "armajitto/defs/arm/flags.hpp"
#include "armajitto/ir/defs/arguments.hpp"
#include "armajitto/ir/defs/memory_access.hpp"
#include "basic_block.hpp"

#include <vector>

namespace armajitto::ir {

struct ALUVarPair {
    Variable lo;
    Variable hi;
};

struct BranchExchangeVars {
    Variable pc;
    Variable cpsr;
};

class Emitter {
public:
    Emitter(BasicBlock &block)
        : m_block(block)
        , m_insertionPoint(block.m_ops.end()) {

        auto loc = m_block.Location();
        m_thumb = loc.IsThumbMode();
        m_currInstrAddr = loc.BaseAddress();
        m_instrSize = m_thumb ? sizeof(uint16_t) : sizeof(uint32_t);
    }

    BasicBlock &GetBlock() {
        return m_block;
    }

    uint32_t InstructionSize() const {
        return m_instrSize;
    }

    bool IsThumbMode() const {
        return m_thumb;
    }

    uint32_t CurrentInstructionAddress() const {
        return m_currInstrAddr;
    }

    uint32_t CurrentPC() const {
        return m_currInstrAddr + 2 * m_instrSize;
    }

    void NextInstruction();
    void SetCondition(arm::Condition cond);

    Variable GetRegister(GPR src);
    void SetRegister(GPR dst, VarOrImmArg src);
    Variable GetCPSR();
    void SetCPSR(VarOrImmArg src);
    Variable GetSPSR();
    void SetSPSR(VarOrImmArg src);

    Variable MemRead(MemAccessMode mode, MemAccessSize size, VarOrImmArg address);
    void MemWrite(MemAccessSize size, VarOrImmArg src, VarOrImmArg address);

    Variable BarrelShifter(const arm::RegisterSpecifiedShift &shift);

    Variable LogicalShiftLeft(VarOrImmArg value, VarOrImmArg amount, bool setFlags);
    Variable LogicalShiftRight(VarOrImmArg value, VarOrImmArg amount, bool setFlags);
    Variable ArithmeticShiftRight(VarOrImmArg value, VarOrImmArg amount, bool setFlags);
    Variable RotateRight(VarOrImmArg value, VarOrImmArg amount, bool setFlags);
    Variable RotateRightExtend(VarOrImmArg value, bool setFlags);

    Variable BitwiseAnd(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    Variable BitwiseOr(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    Variable BitwiseXor(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    Variable BitClear(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    Variable CountLeadingZeros(VarOrImmArg value);

    Variable Add(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    Variable AddCarry(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    Variable Subtract(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    Variable SubtractCarry(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);

    Variable Move(VarOrImmArg value, bool setFlags);
    Variable MoveNegated(VarOrImmArg value, bool setFlags);

    void Test(VarOrImmArg lhs, VarOrImmArg rhs);
    void TestEquivalence(VarOrImmArg lhs, VarOrImmArg rhs);
    void Compare(VarOrImmArg lhs, VarOrImmArg rhs);
    void CompareNegated(VarOrImmArg lhs, VarOrImmArg rhs);

    Variable SaturatingAdd(VarOrImmArg lhs, VarOrImmArg rhs);
    Variable SaturatingSubtract(VarOrImmArg lhs, VarOrImmArg rhs);

    Variable Multiply(VarOrImmArg lhs, VarOrImmArg rhs, bool signedMul, bool setFlags);
    ALUVarPair MultiplyLong(VarOrImmArg lhs, VarOrImmArg rhs, bool signedMul, bool shiftDownHalf, bool setFlags);
    ALUVarPair AddLong(VarOrImmArg lhsLo, VarOrImmArg lhsHi, VarOrImmArg rhsLo, VarOrImmArg rhsHi, bool setFlags);

    void StoreFlags(Flags flags);
    void UpdateFlags(Flags flags);
    void UpdateStickyOverflow();

    Variable Branch(VarOrImmArg srcCPSR, VarOrImmArg address);
    BranchExchangeVars BranchExchange(VarOrImmArg srcCPSR, VarOrImmArg address);
    void LinkBeforeBranch();

    Variable LoadCopRegister(uint8_t cpnum, uint8_t opcode1, uint8_t crn, uint8_t crm, uint8_t opcode2, bool ext);
    void StoreCopRegister(uint8_t cpnum, uint8_t opcode1, uint8_t crn, uint8_t crm, uint8_t opcode2, bool ext,
                          VarOrImmArg srcValue);

    void FetchInstruction();

private:
    BasicBlock &m_block;

    bool m_thumb;
    uint32_t m_currInstrAddr;
    uint32_t m_instrSize;

    // --- Operation manipulators ----------------------------------------------

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

    // --- Helpers -------------------------------------------------------------

    uint32_t m_nextVarID = 0;
    Variable Var();
};

} // namespace armajitto::ir
