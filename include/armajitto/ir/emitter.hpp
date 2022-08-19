#pragma once

#include "armajitto/guest/arm/exceptions.hpp"
#include "armajitto/guest/arm/flags.hpp"
#include "armajitto/ir/defs/arguments.hpp"
#include "armajitto/ir/defs/memory_access.hpp"
#include "basic_block.hpp"

#include <cassert>
#include <vector>

namespace armajitto::ir {

struct ALUVarPair {
    Variable lo;
    Variable hi;
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

    // -----------------------------------------------------------------------------------------------------------------
    // Translator helper functions

    void NextInstruction();
    void SetCondition(arm::Condition cond);

    uint32_t CurrentPC() const {
        return m_currInstrAddr + 2 * m_instrSize;
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Optimizer helper functions

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

    void MoveCursor(int64_t offset) {
        assert(static_cast<size_t>(GetCursorPos() + offset) <= m_block.m_ops.size());
        m_insertionPoint += offset;
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

    Emitter &Overwrite() {
        m_overwriteNext = true;
        return *this;
    }

    void Erase(size_t pos, size_t count = 1) {
        assert(count >= 1);
        auto it = m_block.m_ops.begin() + pos;
        m_block.m_ops.erase(it, it + count);
    }

    void EraseNext(size_t count = 1) {
        assert(count >= 1);
        m_block.m_ops.erase(m_insertionPoint, m_insertionPoint + count);
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Basic IR instruction emitters

    Variable GetRegister(GPRArg src);
    void SetRegister(GPRArg dst, VarOrImmArg src);
    void SetRegisterExceptPC(GPRArg dst, VarOrImmArg src);
    Variable GetCPSR();
    void SetCPSR(VarOrImmArg src);
    Variable GetSPSR();
    void SetSPSR(VarOrImmArg src);
    void SetSPSR(VarOrImmArg src, arm::Mode mode);

    Variable MemRead(MemAccessMode mode, MemAccessSize size, VarOrImmArg address);
    void MemWrite(MemAccessSize size, VarOrImmArg src, VarOrImmArg address);
    void Preload(VarOrImmArg address);

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

    void StoreFlags(Flags flags, VarOrImmArg values);
    void UpdateFlags(Flags flags);
    void UpdateStickyOverflow();

    void Branch(VarOrImmArg address);
    void BranchExchange(VarOrImmArg address);

    Variable LoadCopRegister(uint8_t cpnum, uint8_t opcode1, uint8_t crn, uint8_t crm, uint8_t opcode2, bool ext);
    void StoreCopRegister(uint8_t cpnum, uint8_t opcode1, uint8_t crn, uint8_t crm, uint8_t opcode2, bool ext,
                          VarOrImmArg srcValue);

    Variable Constant(uint32_t value);
    Variable CopyVar(VariableArg var);
    Variable GetBaseVectorAddress();

    // -----------------------------------------------------------------------------------------------------------------
    // Complex IR instruction sequence emitters

    void CopySPSRToCPSR();

    Variable ComputeAddress(const arm::Addressing &addressing);
    Variable ApplyAddressOffset(Variable baseAddress, const arm::Addressing &addressing);
    Variable BarrelShifter(const arm::RegisterSpecifiedShift &shift, bool setFlags);

    void LinkBeforeBranch();

    void EnterException(arm::Exception vector);

    void FetchInstruction();

private:
    BasicBlock &m_block;

    bool m_thumb;
    uint32_t m_currInstrAddr;
    uint32_t m_instrSize;

    // --- Operation manipulators ----------------------------------------------

    bool m_overwriteNext = false;

    using OpIterator = std::vector<std::unique_ptr<IROp>>::iterator;
    OpIterator m_insertionPoint;

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

    // --- Variables -----------------------------------------------------------

    uint32_t m_nextVarID = 0;
    Variable Var();
};

} // namespace armajitto::ir
