#include "armajitto/ir/emitter.hpp"
#include "armajitto/ir/ops/ir_ops.hpp"

namespace armajitto::ir {

Variable Emitter::Var(const char *name) {
    return m_block.m_vars.emplace_back(m_block.m_vars.size(), name);
}

void Emitter::NextInstruction() {
    ++m_block.m_instrCount;
    m_currInstrAddr += m_instrSize;
}

void Emitter::SetCondition(arm::Condition cond) {
    m_block.m_cond = cond;
}

void Emitter::GetRegister(VariableArg dst, GPR src) {
    AppendOp<IRGetRegisterOp>(dst, src);
}

void Emitter::SetRegister(GPR dst, VarOrImmArg src) {
    AppendOp<IRSetRegisterOp>(dst, src);
}

void Emitter::GetCPSR(VariableArg dst) {
    AppendOp<IRGetCPSROp>(dst);
}

void Emitter::SetCPSR(VarOrImmArg src) {
    AppendOp<IRSetCPSROp>(src);
}

void Emitter::GetSPSR(arm::Mode mode, VariableArg dst) {
    AppendOp<IRGetSPSROp>(mode, dst);
}

void Emitter::SetSPSR(arm::Mode mode, VarOrImmArg src) {
    AppendOp<IRSetSPSROp>(mode, src);
}

void Emitter::MemRead(MemAccessMode mode, MemAccessSize size, VariableArg dst, VarOrImmArg address) {
    AppendOp<IRMemReadOp>(mode, size, dst, address);
}

void Emitter::MemWrite(MemAccessSize size, VarOrImmArg src, VarOrImmArg address) {
    AppendOp<IRMemWriteOp>(size, src, address);
}

void Emitter::LogicalShiftLeft(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setFlags) {
    AppendOp<IRLogicalShiftLeftOp>(dst, value, amount, setFlags);
}

void Emitter::LogicalShiftRight(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setFlags) {
    AppendOp<IRLogicalShiftRightOp>(dst, value, amount, setFlags);
}

void Emitter::ArithmeticShiftRight(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setFlags) {
    AppendOp<IRArithmeticShiftRightOp>(dst, value, amount, setFlags);
}

void Emitter::RotateRight(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setFlags) {
    AppendOp<IRRotateRightOp>(dst, value, amount, setFlags);
}

void Emitter::RotateRightExtend(VariableArg dst, VarOrImmArg value, bool setFlags) {
    AppendOp<IRRotateRightExtendOp>(dst, value, setFlags);
}

void Emitter::BitwiseAnd(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    AppendOp<IRBitwiseAndOp>(dst, lhs, rhs, setFlags);
}

void Emitter::BitwiseXor(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    AppendOp<IRBitwiseXorOp>(dst, lhs, rhs, setFlags);
}

void Emitter::Subtract(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    AppendOp<IRSubtractOp>(dst, lhs, rhs, setFlags);
}

void Emitter::ReverseSubtract(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    AppendOp<IRReverseSubtractOp>(dst, lhs, rhs, setFlags);
}

void Emitter::Add(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    AppendOp<IRAddOp>(dst, lhs, rhs, setFlags);
}

void Emitter::AddCarry(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    AppendOp<IRAddCarryOp>(dst, lhs, rhs, setFlags);
}

void Emitter::SubtractCarry(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    AppendOp<IRSubtractCarryOp>(dst, lhs, rhs, setFlags);
}

void Emitter::ReverseSubtractCarry(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    AppendOp<IRReverseSubtractCarryOp>(dst, lhs, rhs, setFlags);
}

void Emitter::BitwiseOr(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    AppendOp<IRBitwiseOrOp>(dst, lhs, rhs, setFlags);
}

void Emitter::Move(VariableArg dst, VarOrImmArg value, bool setFlags) {
    AppendOp<IRMoveOp>(dst, value, setFlags);
}

void Emitter::BitClear(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    AppendOp<IRBitClearOp>(dst, lhs, rhs, setFlags);
}

void Emitter::MoveNegated(VariableArg dst, VarOrImmArg value, bool setFlags) {
    AppendOp<IRMoveNegatedOp>(dst, value, setFlags);
}

void Emitter::CountLeadingZeros(VariableArg dst, VarOrImmArg value) {
    AppendOp<IRCountLeadingZerosOp>(dst, value);
}

void Emitter::SaturatingAdd(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs) {
    AppendOp<IRSaturatingAddOp>(dst, lhs, rhs);
}

void Emitter::SaturatingSubtract(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs) {
    AppendOp<IRSaturatingSubtractOp>(dst, lhs, rhs);
}

void Emitter::Multiply(VariableArg dstLo, VariableArg dstHi, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    AppendOp<IRMultiplyOp>(dstLo, dstHi, lhs, rhs, setFlags);
}

void Emitter::AddLong(VariableArg dstLo, VariableArg dstHi, VarOrImmArg lhsLo, VarOrImmArg lhsHi, VarOrImmArg rhsLo,
                      VarOrImmArg rhsHi, bool setFlags) {
    AppendOp<IRAddLongOp>(dstLo, dstHi, lhsLo, lhsHi, rhsLo, rhsHi, setFlags);
}

void Emitter::StoreFlags(uint8_t mask, VariableArg dstCPSR, VariableArg srcCPSR) {
    AppendOp<IRStoreFlagsOp>(mask, dstCPSR, srcCPSR);
}

void Emitter::UpdateFlags(uint8_t mask, VariableArg dstCPSR, VariableArg srcCPSR) {
    AppendOp<IRUpdateFlagsOp>(mask, dstCPSR, srcCPSR);
}

void Emitter::UpdateStickyOverflow(VariableArg dstCPSR, VariableArg srcCPSR) {
    AppendOp<IRUpdateStickyOverflowOp>(dstCPSR, srcCPSR);
}

void Emitter::Branch(VariableArg dstPC, VarOrImmArg srcCPSR, VarOrImmArg address) {
    AppendOp<IRBranchOp>(dstPC, srcCPSR, address);
}

void Emitter::BranchExchange(VariableArg dstPC, VariableArg dstCPSR, VarOrImmArg srcCPSR, VarOrImmArg address) {
    AppendOp<IRBranchExchangeOp>(dstPC, dstCPSR, srcCPSR, address);
}

void Emitter::LoadCopRegister(VariableArg dstValue, uint8_t cpnum, uint8_t opcode1, uint8_t crn, uint8_t crm,
                              uint8_t opcode2, bool ext) {
    AppendOp<IRLoadCopRegisterOp>(dstValue, cpnum, opcode1, crn, crm, opcode2, ext);
}

void Emitter::StoreCopRegister(VarOrImmArg srcValue, uint8_t cpnum, uint8_t opcode1, uint8_t crn, uint8_t crm,
                               uint8_t opcode2, bool ext) {
    AppendOp<IRStoreCopRegisterOp>(srcValue, cpnum, opcode1, crn, crm, opcode2, ext);
}

void Emitter::FetchInstruction() {
    SetRegister(GPR::PC, CurrentPC() + m_instrSize);
    // TODO: cycle counting
}

} // namespace armajitto::ir
