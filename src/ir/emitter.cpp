#include "armajitto/ir/emitter.hpp"
#include "armajitto/ir/ops/ir_ops.hpp"

namespace armajitto::ir {

void Emitter::NextInstruction() {
    ++m_block.m_instrCount;
    m_currInstrAddr += m_instrSize;
}

void Emitter::SetCondition(arm::Condition cond) {
    m_block.m_cond = cond;
}

Variable Emitter::GetRegister(GPR src) {
    auto dst = Var();
    AppendOp<IRGetRegisterOp>(dst, src);
    return dst;
}

void Emitter::SetRegister(GPR dst, VarOrImmArg src) {
    AppendOp<IRSetRegisterOp>(dst, src);
}

Variable Emitter::GetCPSR() {
    auto dst = Var();
    AppendOp<IRGetCPSROp>(dst);
    return dst;
}

void Emitter::SetCPSR(VarOrImmArg src) {
    AppendOp<IRSetCPSROp>(src);
}

Variable Emitter::GetSPSR() {
    auto dst = Var();
    AppendOp<IRGetSPSROp>(m_block.Location().Mode(), dst);
    return dst;
}

void Emitter::SetSPSR(VarOrImmArg src) {
    AppendOp<IRSetSPSROp>(m_block.Location().Mode(), src);
}

Variable Emitter::MemRead(MemAccessMode mode, MemAccessSize size, VarOrImmArg address) {
    auto dst = Var();
    AppendOp<IRMemReadOp>(mode, size, dst, address);
    return dst;
}

void Emitter::MemWrite(MemAccessSize size, VarOrImmArg src, VarOrImmArg address) {
    AppendOp<IRMemWriteOp>(size, src, address);
}

Variable Emitter::BarrelShifter(const arm::RegisterSpecifiedShift &shift) {
    auto value = GetRegister(shift.srcReg);

    VarOrImmArg amount;
    if (shift.immediate) {
        amount = shift.amount.imm;
    } else {
        amount = GetRegister(shift.amount.reg);
        // TODO: add one I cycle
    }

    Variable result{};
    switch (shift.type) {
    case arm::ShiftType::LSL:
        if (shift.immediate && shift.amount.imm == 0) {
            result = value;
        } else {
            result = LogicalShiftLeft(value, amount, true);
        }
        break;
    case arm::ShiftType::LSR: result = LogicalShiftRight(value, amount, true); break;
    case arm::ShiftType::ASR: result = ArithmeticShiftRight(value, amount, true); break;
    case arm::ShiftType::ROR:
        if (shift.immediate && shift.amount.imm == 0) {
            result = RotateRightExtend(value, true);
        } else {
            result = RotateRight(value, amount, true);
        }
        break;
    }
    return result;
}

Variable Emitter::LogicalShiftLeft(VarOrImmArg value, VarOrImmArg amount, bool setFlags) {
    auto dst = Var();
    AppendOp<IRLogicalShiftLeftOp>(dst, value, amount, setFlags);
    return dst;
}

Variable Emitter::LogicalShiftRight(VarOrImmArg value, VarOrImmArg amount, bool setFlags) {
    auto dst = Var();
    AppendOp<IRLogicalShiftRightOp>(dst, value, amount, setFlags);
    return dst;
}

Variable Emitter::ArithmeticShiftRight(VarOrImmArg value, VarOrImmArg amount, bool setFlags) {
    auto dst = Var();
    AppendOp<IRArithmeticShiftRightOp>(dst, value, amount, setFlags);
    return dst;
}

Variable Emitter::RotateRight(VarOrImmArg value, VarOrImmArg amount, bool setFlags) {
    auto dst = Var();
    AppendOp<IRRotateRightOp>(dst, value, amount, setFlags);
    return dst;
}

Variable Emitter::RotateRightExtend(VarOrImmArg value, bool setFlags) {
    auto dst = Var();
    AppendOp<IRRotateRightExtendOp>(dst, value, setFlags);
    return dst;
}

Variable Emitter::BitwiseAnd(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    AppendOp<IRBitwiseAndOp>(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::BitwiseOr(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    AppendOp<IRBitwiseOrOp>(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::BitwiseXor(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    AppendOp<IRBitwiseXorOp>(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::BitClear(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    AppendOp<IRBitClearOp>(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::CountLeadingZeros(VarOrImmArg value) {
    auto dst = Var();
    AppendOp<IRCountLeadingZerosOp>(dst, value);
    return dst;
}

Variable Emitter::Add(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    AppendOp<IRAddOp>(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::AddCarry(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    AppendOp<IRAddCarryOp>(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::Subtract(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    AppendOp<IRSubtractOp>(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::SubtractCarry(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    AppendOp<IRSubtractCarryOp>(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::Move(VarOrImmArg value, bool setFlags) {
    auto dst = Var();
    AppendOp<IRMoveOp>(dst, value, setFlags);
    return dst;
}

Variable Emitter::MoveNegated(VarOrImmArg value, bool setFlags) {
    auto dst = Var();
    AppendOp<IRMoveNegatedOp>(dst, value, setFlags);
    return dst;
}

void Emitter::Test(VarOrImmArg lhs, VarOrImmArg rhs) {
    AppendOp<IRBitwiseAndOp>(lhs, rhs);
}

void Emitter::TestEquivalence(VarOrImmArg lhs, VarOrImmArg rhs) {
    AppendOp<IRBitwiseXorOp>(lhs, rhs);
}

void Emitter::Compare(VarOrImmArg lhs, VarOrImmArg rhs) {
    AppendOp<IRSubtractOp>(lhs, rhs);
}

void Emitter::CompareNegated(VarOrImmArg lhs, VarOrImmArg rhs) {
    AppendOp<IRAddOp>(lhs, rhs);
}

Variable Emitter::SaturatingAdd(VarOrImmArg lhs, VarOrImmArg rhs) {
    auto dst = Var();
    AppendOp<IRSaturatingAddOp>(dst, lhs, rhs);
    return dst;
}

Variable Emitter::SaturatingSubtract(VarOrImmArg lhs, VarOrImmArg rhs) {
    auto dst = Var();
    AppendOp<IRSaturatingSubtractOp>(dst, lhs, rhs);
    return dst;
}

Variable Emitter::Multiply(VarOrImmArg lhs, VarOrImmArg rhs, bool signedMul, bool setFlags) {
    auto dst = Var();
    AppendOp<IRMultiplyOp>(dst, lhs, rhs, signedMul, setFlags);
    return dst;
}

ALUVarPair Emitter::MultiplyLong(VarOrImmArg lhs, VarOrImmArg rhs, bool signedMul, bool shiftDownHalf, bool setFlags) {
    auto dstLo = Var();
    auto dstHi = Var();
    AppendOp<IRMultiplyLongOp>(dstLo, dstHi, lhs, rhs, signedMul, shiftDownHalf, setFlags);
    return {dstLo, dstHi};
}

ALUVarPair Emitter::AddLong(VarOrImmArg lhsLo, VarOrImmArg lhsHi, VarOrImmArg rhsLo, VarOrImmArg rhsHi, bool setFlags) {
    auto dstLo = Var();
    auto dstHi = Var();
    AppendOp<IRAddLongOp>(dstLo, dstHi, lhsLo, lhsHi, rhsLo, rhsHi, setFlags);
    return {dstLo, dstHi};
}

void Emitter::StoreFlags(Flags flags) {
    auto srcCPSR = GetCPSR();
    auto dstCPSR = Var();
    AppendOp<IRStoreFlagsOp>(flags, dstCPSR, srcCPSR);
    SetCPSR(dstCPSR);
}

void Emitter::UpdateFlags(Flags flags) {
    auto srcCPSR = GetCPSR();
    auto dstCPSR = Var();
    AppendOp<IRUpdateFlagsOp>(flags, dstCPSR, srcCPSR);
    SetCPSR(dstCPSR);
}

void Emitter::UpdateStickyOverflow() {
    auto srcCPSR = GetCPSR();
    auto dstCPSR = Var();
    AppendOp<IRUpdateStickyOverflowOp>(dstCPSR, srcCPSR);
    SetCPSR(dstCPSR);
}

Variable Emitter::Branch(VarOrImmArg srcCPSR, VarOrImmArg address) {
    auto dstPC = Var();
    AppendOp<IRBranchOp>(dstPC, srcCPSR, address);
    return dstPC;
}

BranchExchangeVars Emitter::BranchExchange(VarOrImmArg srcCPSR, VarOrImmArg address) {
    auto dstPC = Var();
    auto dstCPSR = Var();
    AppendOp<IRBranchExchangeOp>(dstPC, dstCPSR, srcCPSR, address);
    return {dstPC, dstCPSR};
}

void Emitter::LinkBeforeBranch() {
    uint32_t linkAddress = m_currInstrAddr + m_instrSize;
    if (m_thumb) {
        linkAddress |= 1;
    }
    SetRegister(GPR::LR, linkAddress);
}

Variable Emitter::LoadCopRegister(uint8_t cpnum, uint8_t opcode1, uint8_t crn, uint8_t crm, uint8_t opcode2, bool ext) {
    auto dstValue = Var();
    AppendOp<IRLoadCopRegisterOp>(dstValue, cpnum, opcode1, crn, crm, opcode2, ext);
    return dstValue;
}

void Emitter::StoreCopRegister(uint8_t cpnum, uint8_t opcode1, uint8_t crn, uint8_t crm, uint8_t opcode2, bool ext,
                               VarOrImmArg srcValue) {
    AppendOp<IRStoreCopRegisterOp>(srcValue, cpnum, opcode1, crn, crm, opcode2, ext);
}

void Emitter::FetchInstruction() {
    SetRegister(GPR::PC, CurrentPC() + m_instrSize);
    // TODO: cycle counting
}

Variable Emitter::Var() {
    return Variable{m_nextVarID++};
}

} // namespace armajitto::ir
