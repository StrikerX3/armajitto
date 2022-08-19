#pragma once

#include "armajitto/guest/arm/exceptions.hpp"
#include "armajitto/guest/arm/flags.hpp"
#include "armajitto/ir/defs/arguments.hpp"
#include "armajitto/ir/defs/memory_access.hpp"
#include "basic_block.hpp"

#include <vector>

namespace armajitto::ir {

struct ALUVarPair {
    Variable lo;
    Variable hi;
};

class Emitter {
public:
    Emitter(BasicBlock &block)
        : m_blockWriter(block) {

        auto loc = block.Location();
        m_thumb = loc.IsThumbMode();
        m_mode = loc.Mode();
        m_currInstrAddr = loc.BaseAddress();
        m_instrSize = m_thumb ? sizeof(uint16_t) : sizeof(uint32_t);
    }

    BasicBlock &GetBlock() {
        return m_blockWriter.Block();
    }

    uint32_t InstructionSize() const {
        return m_instrSize;
    }

    bool IsThumbMode() const {
        return m_thumb;
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Translator helper functions
    // TODO: figure out a way to expose these methods only to the translator

    void NextInstruction();
    void SetCondition(arm::Condition cond);

    uint32_t CurrentPC() const {
        return m_currInstrAddr + 2 * m_instrSize;
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Optimizer helper functions

    size_t GetCodeSize() const {
        return m_blockWriter.GetCodeSize();
    }

    size_t GetCursorPos() const {
        return m_blockWriter.GetCursorPos();
    }

    bool IsCursorAtEnd() const {
        return m_blockWriter.IsCursorAtEnd();
    }

    void SetCursorPos(size_t index) {
        m_blockWriter.SetCursorPos(index);
    }

    size_t MoveCursor(int64_t offset) {
        return m_blockWriter.MoveCursor(offset);
    }

    IROp &GetOp(size_t index) {
        return m_blockWriter.GetOp(index);
    }

    IROp *GetCurrentOp() {
        return m_blockWriter.GetCurrentOp();
    }

    Emitter &Overwrite() {
        m_blockWriter.OverwriteNext();
        return *this;
    }

    void Erase(size_t pos, size_t count = 1) {
        m_blockWriter.Erase(pos, count);
    }

    void EraseNext(size_t count = 1) {
        m_blockWriter.EraseNext(count);
    }

    bool IsModifiedSinceLastCursorMove() const {
        return m_blockWriter.IsModifiedSinceLastCursorMove();
    }

    void ClearModifiedSinceLastCursorMove() {
        m_blockWriter.ClearModifiedSinceLastCursorMove();
    }

    bool IsDirty() const {
        return m_blockWriter.IsDirty();
    }

    void ClearDirtyFlag() {
        m_blockWriter.ClearDirtyFlag();
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

    void SetNZ(uint32_t value);
    void SetNZ(uint64_t value);
    void SetNZCV(uint32_t value, bool carry, bool overflow);

    void Branch(VarOrImmArg address);
    void BranchExchange(VarOrImmArg address);

    Variable LoadCopRegister(uint8_t cpnum, uint8_t opcode1, uint8_t crn, uint8_t crm, uint8_t opcode2, bool ext);
    void StoreCopRegister(uint8_t cpnum, uint8_t opcode1, uint8_t crn, uint8_t crm, uint8_t opcode2, bool ext,
                          VarOrImmArg srcValue);

    void Constant(VariableArg dst, uint32_t value);
    Variable Constant(uint32_t value);
    void CopyVar(VariableArg dst, VariableArg var);
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
    BasicBlock::Writer m_blockWriter;

    bool m_thumb;
    arm::Mode m_mode;
    uint32_t m_instrSize;

    uint32_t m_currInstrAddr;

    // --- Variables -----------------------------------------------------------

    Variable Var();
};

} // namespace armajitto::ir
