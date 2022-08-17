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

Variable Emitter::GetSPSR(arm::Mode mode) {
    auto dst = Var();
    AppendOp<IRGetSPSROp>(mode, dst);
    return dst;
}

void Emitter::SetSPSR(arm::Mode mode, VarOrImmArg src) {
    AppendOp<IRSetSPSROp>(mode, src);
}

Variable Emitter::MemRead(MemAccessMode mode, MemAccessSize size, VarOrImmArg address) {
    auto dst = Var();
    AppendOp<IRMemReadOp>(mode, size, dst, address);
    return dst;
}

void Emitter::MemWrite(MemAccessSize size, VarOrImmArg src, VarOrImmArg address) {
    AppendOp<IRMemWriteOp>(size, src, address);
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

Variable Emitter::BitwiseXor(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    AppendOp<IRBitwiseXorOp>(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::Subtract(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    AppendOp<IRSubtractOp>(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::ReverseSubtract(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    AppendOp<IRReverseSubtractOp>(dst, lhs, rhs, setFlags);
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

Variable Emitter::SubtractCarry(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    AppendOp<IRSubtractCarryOp>(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::ReverseSubtractCarry(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    AppendOp<IRReverseSubtractCarryOp>(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::BitwiseOr(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    AppendOp<IRBitwiseOrOp>(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::Move(VarOrImmArg value, bool setFlags) {
    auto dst = Var();
    AppendOp<IRMoveOp>(dst, value, setFlags);
    return dst;
}

Variable Emitter::BitClear(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    AppendOp<IRBitClearOp>(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::MoveNegated(VarOrImmArg value, bool setFlags) {
    auto dst = Var();
    AppendOp<IRMoveNegatedOp>(dst, value, setFlags);
    return dst;
}

Variable Emitter::CountLeadingZeros(VarOrImmArg value) {
    auto dst = Var();
    AppendOp<IRCountLeadingZerosOp>(dst, value);
    return dst;
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

ALUVarPair Emitter::Multiply(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dstLo = Var();
    auto dstHi = Var();
    AppendOp<IRMultiplyOp>(dstLo, dstHi, lhs, rhs, setFlags);
    return {dstLo, dstHi};
}

ALUVarPair Emitter::AddLong(VarOrImmArg lhsLo, VarOrImmArg lhsHi, VarOrImmArg rhsLo, VarOrImmArg rhsHi, bool setFlags) {
    auto dstLo = Var();
    auto dstHi = Var();
    AppendOp<IRAddLongOp>(dstLo, dstHi, lhsLo, lhsHi, rhsLo, rhsHi, setFlags);
    return {dstLo, dstHi};
}

Variable Emitter::StoreFlags(uint8_t mask, VariableArg srcCPSR) {
    auto dstCPSR = Var();
    AppendOp<IRStoreFlagsOp>(mask, dstCPSR, srcCPSR);
    return dstCPSR;
}

Variable Emitter::UpdateFlags(uint8_t mask, VariableArg srcCPSR) {
    auto dstCPSR = Var();
    AppendOp<IRUpdateFlagsOp>(mask, dstCPSR, srcCPSR);
    return dstCPSR;
}

Variable Emitter::UpdateStickyOverflow(VariableArg srcCPSR) {
    auto dstCPSR = Var();
    AppendOp<IRUpdateStickyOverflowOp>(dstCPSR, srcCPSR);
    return dstCPSR;
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
