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

Variable Emitter::GetRegister(GPRArg src) {
    if (src.gpr == GPR::PC) {
        return Constant(CurrentPC());
    } else {
        auto dst = Var();
        AppendOp<IRGetRegisterOp>(dst, src);
        return dst;
    }
}

void Emitter::SetRegister(GPRArg dst, VarOrImmArg src) {
    AppendOp<IRSetRegisterOp>(dst, src);
}

void Emitter::SetRegisterExceptPC(GPRArg dst, VarOrImmArg src) {
    if (dst.gpr != GPR::PC) {
        SetRegister(dst, src);
    }
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
    AppendOp<IRGetSPSROp>(dst, m_block.Location().Mode());
    return dst;
}

void Emitter::SetSPSR(VarOrImmArg src) {
    SetSPSR(src, m_block.Location().Mode());
}

void Emitter::SetSPSR(VarOrImmArg src, arm::Mode mode) {
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

void Emitter::Preload(VarOrImmArg address) {
    AppendOp<IRPreloadOp>(address);
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

void Emitter::StoreFlags(Flags flags, VarOrImmArg values) {
    auto srcCPSR = GetCPSR();
    auto dstCPSR = Var();
    AppendOp<IRStoreFlagsOp>(flags, dstCPSR, srcCPSR, values);
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

void Emitter::Branch(VarOrImmArg address) {
    AppendOp<IRBranchOp>(address);
}

void Emitter::BranchExchange(VarOrImmArg address) {
    AppendOp<IRBranchExchangeOp>(address);
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

Variable Emitter::Constant(uint32_t value) {
    auto dst = Var();
    AppendOp<IRConstantOp>(dst, value);
    return dst;
}

Variable Emitter::CopyVar(VariableArg var) {
    auto dst = Var();
    AppendOp<IRCopyVarOp>(dst, var);
    return dst;
}

Variable Emitter::GetBaseVectorAddress() {
    auto dst = Var();
    AppendOp<IRGetBaseVectorAddressOp>(dst);
    return dst;
}

void Emitter::CopySPSRToCPSR() {
    auto spsr = GetSPSR();
    SetCPSR(spsr);
}

Variable Emitter::ComputeAddress(const arm::Addressing &addressing) {
    auto baseReg = GetRegister(addressing.baseReg);
    return ApplyAddressOffset(baseReg, addressing);
}

Variable Emitter::ApplyAddressOffset(Variable baseAddress, const arm::Addressing &addressing) {
    if (addressing.immediate) {
        if (addressing.immValue == 0) {
            return baseAddress;
        } else {
            if (addressing.positiveOffset) {
                return Add(baseAddress, addressing.immValue, false);
            } else {
                return Subtract(baseAddress, addressing.immValue, false);
            }
        }
    } else {
        auto shiftValue = BarrelShifter(addressing.shift, false);
        if (addressing.positiveOffset) {
            return Add(baseAddress, shiftValue, false);
        } else {
            return Subtract(baseAddress, shiftValue, false);
        }
    }
}

Variable Emitter::BarrelShifter(const arm::RegisterSpecifiedShift &shift, bool setFlags) {
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
            result = LogicalShiftLeft(value, amount, setFlags);
        }
        break;
    case arm::ShiftType::LSR: result = LogicalShiftRight(value, amount, setFlags); break;
    case arm::ShiftType::ASR: result = ArithmeticShiftRight(value, amount, setFlags); break;
    case arm::ShiftType::ROR:
        if (shift.immediate && shift.amount.imm == 0) {
            result = RotateRightExtend(value, setFlags);
        } else {
            result = RotateRight(value, amount, setFlags);
        }
        break;
    }
    return result;
}

void Emitter::LinkBeforeBranch() {
    uint32_t linkAddress = m_currInstrAddr + m_instrSize;
    if (m_thumb) {
        linkAddress |= 1;
    }
    SetRegister(GPR::LR, linkAddress);
}

void Emitter::EnterException(arm::Exception vector) {
    const auto &vectorInfo = arm::kExceptionVectorInfos[static_cast<size_t>(vector)];
    const auto nn = m_thumb ? vectorInfo.thumbOffset : vectorInfo.armOffset;

    uint32_t setBits = static_cast<uint32_t>(vectorInfo.mode) | (1 << 7); // Set I and mode bits
    if (vectorInfo.F) {
        setBits |= (1 << 6); // Set F bit
    }

    auto cpsr = GetCPSR();
    SetSPSR(cpsr, vectorInfo.mode);
    cpsr = BitClear(cpsr, 0b11'1111, false); // Clear T and mode bits
    cpsr = BitwiseOr(cpsr, setBits, false);  // Set I, F and mode bits
    SetCPSR(cpsr);

    auto pc = Add(GetBaseVectorAddress(), static_cast<uint32_t>(vector) * 4 + sizeof(uint32_t) * 2, false);
    SetRegister(GPR::LR, m_currInstrAddr + nn);
    SetRegister(GPR::PC, pc);
}

void Emitter::FetchInstruction() {
    SetRegister(GPR::PC, CurrentPC() + m_instrSize);
}

Variable Emitter::Var() {
    return Variable{m_nextVarID++};
}

} // namespace armajitto::ir
