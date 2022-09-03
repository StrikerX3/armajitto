#pragma once

#include "armajitto/core/pmr_allocator.hpp"
#include "armajitto/guest/arm/coprocessor.hpp"
#include "armajitto/guest/arm/exceptions.hpp"
#include "armajitto/guest/arm/flags.hpp"
#include "armajitto/ir/defs/arguments.hpp"
#include "armajitto/ir/defs/memory_access.hpp"
#include "basic_block.hpp"

#include <set>
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
        m_basePC = loc.PC();
        m_thumb = loc.IsThumbMode();
        m_mode = loc.Mode();
        m_instrSize = m_thumb ? sizeof(uint16_t) : sizeof(uint32_t);
    }

    BasicBlock &GetBlock() {
        return m_block;
    }

    uint32_t BaseAddress() const {
        return m_basePC - m_instrSize * 2;
    }

    uint32_t BasePC() const {
        return m_basePC;
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

    uint32_t VariableCount() const {
        return m_block.VariableCount();
    }

    // -----------------------------------------------------------------------------------------------------------------
    // Translator helper functions
    // TODO: figure out a way to expose these methods only to the translator

    void NextInstruction();
    void SetCondition(arm::Condition cond);

    // -----------------------------------------------------------------------------------------------------------------
    // Optimizer helper functions
    // TODO: figure out a way to expose these methods only to the optimizers

    // Determines if the emitter has made any changes.
    bool IsDirty() const {
        return m_dirty;
    }

    // Clears the dirty flag.
    void ClearDirtyFlag() {
        m_dirty = false;
    }

    // Moves the emitter's cursor to the head of the IR block.
    void GoToHead() {
        m_currOp = m_block.Head();
        m_overwriteNext = false;
        m_prependNext = false;
        m_currOpErased = false;
    }

    // Moves the emitter's cursor to the tail of the IR block.
    void GoToTail() {
        m_currOp = m_block.Tail();
        m_overwriteNext = false;
        m_prependNext = false;
        m_currOpErased = false;
    }

    struct GoToGuard {
        GoToGuard(IROp *prev, IROp *&currOpRef)
            : m_prev(prev)
            , m_currOpRef(currOpRef) {}

        ~GoToGuard() {
            m_currOpRef = m_prev;
        }

    private:
        IROp *m_prev;
        IROp *&m_currOpRef;
    };

    // Temporarily moves the cursor to the specified IR opcode.
    GoToGuard GoTo(IROp *op) {
        IROp *prev = m_currOp;
        m_currOp = op;
        return {prev, m_currOp};
    }

    // Retrieves the current IR opcode.
    IROp *GetCurrentOp() {
        return m_currOp;
    }

    // Moves the emitter to the next IR opcode in the sequence, if any.
    void NextOp() {
        if (m_currOp != nullptr) {
            if (m_prependNext) {
                m_prependNext = false;
            } else {
                m_currOp = m_currOp->Next();
            }
        }
    }

    // Moves the emitter to the previous IR opcode in the sequence, if any.
    void PrevOp() {
        if (m_currOp != nullptr) {
            // TODO: check if this is correct
            m_currOp = m_currOp->Prev();
        }
    }

    // Signals the emitter to overwrite the current instruction with the next emitted instruction.
    Emitter &Overwrite() {
        m_overwriteNext = true;
        return *this;
    }

    // Erases the specified instruction.
    void Erase(IROp *op) {
        if (op == nullptr) {
            return;
        }
        IROp *result = m_block.Erase(op);
        if (op == m_currOp) {
            m_currOp = result;
            m_prependNext = true;
            m_currOpErased = true;
        }
        m_dirty = true;
    }

    // Determines if the current op was erased.
    bool WasCurrentOpErased() const {
        return m_currOpErased;
    }

    // Rename all variables in the block from scratch, eliminating all gaps in the sequence.
    void RenameVariables() {
        m_block.RenameVariables();
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

    Variable MemRead(MemAccessBus bus, MemAccessMode mode, MemAccessSize size, VarOrImmArg address);
    void MemWrite(MemAccessSize size, VarOrImmArg src, VarOrImmArg address);
    void Preload(VarOrImmArg address);

    Variable LogicalShiftLeft(VarOrImmArg value, VarOrImmArg amount, bool setFlags);
    Variable LogicalShiftRight(VarOrImmArg value, VarOrImmArg amount, bool setFlags);
    Variable ArithmeticShiftRight(VarOrImmArg value, VarOrImmArg amount, bool setFlags);
    Variable RotateRight(VarOrImmArg value, VarOrImmArg amount, bool setFlags);
    Variable RotateRightExtended(VarOrImmArg value, bool setFlags);

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

    Variable SaturatingAdd(VarOrImmArg lhs, VarOrImmArg rhs, bool setQ);
    Variable SaturatingSubtract(VarOrImmArg lhs, VarOrImmArg rhs, bool setQ);

    Variable Multiply(VarOrImmArg lhs, VarOrImmArg rhs, bool signedMul, bool setFlags);
    ALUVarPair MultiplyLong(VarOrImmArg lhs, VarOrImmArg rhs, bool signedMul, bool shiftDownHalf, bool setFlags);
    ALUVarPair AddLong(VarOrImmArg lhsLo, VarOrImmArg lhsHi, VarOrImmArg rhsLo, VarOrImmArg rhsHi, bool setFlags);

    void StoreFlags(arm::Flags flags, VarOrImmArg values);
    void StoreFlags(arm::Flags flags, arm::Flags values);
    void LoadFlags(arm::Flags flags);
    void LoadStickyOverflow();

    arm::Flags SetNZ(uint32_t value);
    arm::Flags SetNZ(uint64_t value);
    arm::Flags SetNZCV(uint32_t value, bool carry, bool overflow);

    void Branch(VarOrImmArg address);
    void BranchExchange(VarOrImmArg address);
    void BranchExchangeL4(VarOrImmArg address);

    Variable LoadCopRegister(uint8_t cpnum, arm::CopRegister reg, bool ext);
    void StoreCopRegister(uint8_t cpnum, arm::CopRegister reg, bool ext, VarOrImmArg srcValue);

    Variable Constant(uint32_t value);
    Variable CopyVar(VariableArg var);
    Variable GetBaseVectorAddress();

    // -------------------------------------------------------------------------
    // Basic IR instruction emitters with destination variables

    void GetRegister(VariableArg dst, GPRArg src);
    void GetRegister(VariableArg dst, arm::GPR src); // using current mode

    void GetCPSR(VariableArg dst);
    void GetSPSR(VariableArg dst);

    void MemRead(MemAccessBus bus, MemAccessMode mode, MemAccessSize size, VariableArg dst, VarOrImmArg address);

    void LogicalShiftLeft(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setFlags);
    void LogicalShiftRight(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setFlags);
    void ArithmeticShiftRight(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setFlags);
    void RotateRight(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setFlags);
    void RotateRightExtended(VariableArg dst, VarOrImmArg value, bool setFlags);

    void BitwiseAnd(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    void BitwiseOr(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    void BitwiseXor(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    void BitClear(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    void CountLeadingZeros(VariableArg dst, VarOrImmArg value);

    void Add(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    void AddCarry(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    void Subtract(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);
    void SubtractCarry(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags);

    void Move(VariableArg dst, VarOrImmArg value, bool setFlags);
    void MoveNegated(VariableArg dst, VarOrImmArg value, bool setFlags);

    void SaturatingAdd(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setQ);
    void SaturatingSubtract(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setQ);

    void Multiply(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool signedMul, bool setFlags);
    void MultiplyLong(VariableArg dstLo, VariableArg dstHi, VarOrImmArg lhs, VarOrImmArg rhs, bool signedMul,
                      bool shiftDownHalf, bool setFlags);
    void AddLong(VariableArg dstLo, VariableArg dstHi, VarOrImmArg lhsLo, VarOrImmArg lhsHi, VarOrImmArg rhsLo,
                 VarOrImmArg rhsHi, bool setFlags);

    void LoadCopRegister(VariableArg dstValue, uint8_t cpnum, arm::CopRegister reg, bool ext);

    void Constant(VariableArg dst, uint32_t value);
    void CopyVar(VariableArg dst, VariableArg var);
    void GetBaseVectorAddress(VariableArg dst);

    // -------------------------------------------------------------------------
    // Complex IR instruction sequence emitters and special cases

    // add.v <lhs>, <rhs> -- specifically for multiply-accumulate affecting Q flag
    Variable AddQ(VarOrImmArg lhs, VarOrImmArg rhs);

    Variable GetOffsetFromCurrentInstructionAddress(int32_t offset);

    void CopySPSRToCPSR();

    Variable ComputeAddress(const arm::Addressing &addressing);
    Variable ApplyAddressOffset(Variable baseAddress, const arm::Addressing &addressing);
    Variable BarrelShifter(const arm::RegisterSpecifiedShift &shift, bool setFlags);

    void LinkBeforeBranch();

    void EnterException(arm::Exception vector);

    void FetchInstruction();

    // -------------------------------------------------------------------------
    // Terminals

    void TerminateDirectLink(uint32_t targetAddress, arm::Mode mode, bool thumb);
    void TerminateIndirectLink();
    void TerminateContinueExecution();
    void TerminateReturn();

private:
    BasicBlock &m_block;

    uint32_t m_basePC;
    bool m_thumb;
    arm::Mode m_mode;
    uint32_t m_instrSize;

    bool m_dirty = false;

    IROp *m_currOp;

    bool m_currOpErased = false;
    bool m_overwriteNext = false;
    bool m_prependNext = false;

    template <typename T, typename... Args>
    void Write(Args &&...args) {
        if (m_overwriteNext) {
            m_currOp = m_block.ReplaceOp<T>(m_currOp, std::forward<Args>(args)...);
            m_overwriteNext = false;
            m_prependNext = false;
        } else if (m_prependNext) {
            m_currOp = m_block.PrependOp<T>(m_currOp, std::forward<Args>(args)...);
            m_prependNext = false;
        } else {
            m_currOp = m_block.AppendOp<T>(m_currOp, std::forward<Args>(args)...);
        }
        m_dirty = true;
    }

    Variable Var();
};

} // namespace armajitto::ir
