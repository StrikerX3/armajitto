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
        : m_block(block)
        , m_currOp(block.Tail()) {

        auto loc = block.Location();
        m_baseAddress = loc.BaseAddress();
        m_thumb = loc.IsThumbMode();
        m_mode = loc.Mode();
        m_currInstrAddr = loc.BaseAddress();
        m_instrSize = m_thumb ? sizeof(uint16_t) : sizeof(uint32_t);
    }

    BasicBlock &GetBlock() {
        return m_block;
    }

    uint32_t BaseAddress() const {
        return m_baseAddress;
    }

    uint32_t BasePC() const {
        return m_baseAddress + m_instrSize * 2;
    }

    uint32_t InstructionSize() const {
        return m_instrSize;
    }

    arm::Mode Mode() const {
        return m_mode;
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

    void ClearDirtyFlag() {
        m_dirty = false;
    }

    bool IsDirty() const {
        return m_dirty;
    }

    void GoToHead() {
        m_currOp = m_block.Head();
    }

    IROp *GetCurrentOp() {
        return m_currOp;
    }

    void NextOp() {
        if (m_currOp != nullptr) {
            m_currOp = m_currOp->Next();
        }
    }

    Emitter &Overwrite() {
        m_overwriteNext = true;
        return *this;
    }

    void Erase(IROp *op) {
        m_block.Remove(op);
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Basic IR instruction emitters

    Variable GetRegister(GPRArg src);
    Variable GetRegister(arm::GPR src); // using current mode
    void SetRegister(GPRArg dst, VarOrImmArg src);
    void SetRegister(arm::GPR dst, VarOrImmArg src); // using current mode
    void SetRegisterExceptPC(GPRArg dst, VarOrImmArg src);
    void SetRegisterExceptPC(arm::GPR dst, VarOrImmArg src); // using current mode
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
    BasicBlock &m_block;

    uint32_t m_baseAddress;
    bool m_thumb;
    arm::Mode m_mode;
    uint32_t m_instrSize;

    uint32_t m_currInstrAddr;

    IROp *m_currOp;
    bool m_dirty = false;
    bool m_overwriteNext = false;

    template <typename T, typename... Args>
    void Write(Args &&...args) {
        if (m_overwriteNext) {
            m_currOp = m_block.ReplaceOp<T>(m_currOp, std::forward<Args>(args)...);
            m_overwriteNext = false;
        } else {
            m_currOp = m_block.AppendOp<T>(m_currOp, std::forward<Args>(args)...);
        }
        m_dirty = true;
    }

    // --- Variables -----------------------------------------------------------

    Variable Var();
};

} // namespace armajitto::ir
