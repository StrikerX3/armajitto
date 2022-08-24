#include "armajitto/ir/emitter.hpp"
#include "armajitto/ir/ir_ops.hpp"

namespace armajitto::ir {

void Emitter::NextInstruction() {
    m_block.NextInstruction();
}

void Emitter::SetCondition(arm::Condition cond) {
    m_block.SetCondition(cond);
}

Variable Emitter::GetRegister(GPRArg src) {
    auto dst = Var();
    Write<IRGetRegisterOp>(dst, src);
    return dst;
}

Variable Emitter::GetRegister(arm::GPR src) {
    return GetRegister({src, m_mode});
}

void Emitter::SetRegister(GPRArg dst, VarOrImmArg src) {
    Write<IRSetRegisterOp>(dst, src);
}

void Emitter::SetRegister(arm::GPR dst, VarOrImmArg src) {
    SetRegister({dst, m_mode}, src);
}

void Emitter::SetRegisterExceptPC(GPRArg dst, VarOrImmArg src) {
    if (dst.gpr != arm::GPR::PC) {
        SetRegister(dst, src);
    }
}

void Emitter::SetRegisterExceptPC(arm::GPR dst, VarOrImmArg src) {
    SetRegisterExceptPC({dst, m_mode}, src);
}

Variable Emitter::GetCPSR() {
    auto dst = Var();
    Write<IRGetCPSROp>(dst);
    return dst;
}

void Emitter::SetCPSR(VarOrImmArg src) {
    Write<IRSetCPSROp>(src);
}

Variable Emitter::GetSPSR() {
    auto dst = Var();
    Write<IRGetSPSROp>(dst, m_mode);
    return dst;
}

void Emitter::SetSPSR(VarOrImmArg src) {
    SetSPSR(src, m_mode);
}

void Emitter::SetSPSR(VarOrImmArg src, arm::Mode mode) {
    Write<IRSetSPSROp>(mode, src);
}

Variable Emitter::MemRead(MemAccessMode mode, MemAccessSize size, VarOrImmArg address) {
    auto dst = Var();
    Write<IRMemReadOp>(mode, size, dst, address);
    return dst;
}

void Emitter::MemWrite(MemAccessSize size, VarOrImmArg src, VarOrImmArg address) {
    Write<IRMemWriteOp>(size, src, address);
}

void Emitter::Preload(VarOrImmArg address) {
    Write<IRPreloadOp>(address);
}

Variable Emitter::LogicalShiftLeft(VarOrImmArg value, VarOrImmArg amount, bool setFlags) {
    auto dst = Var();
    Write<IRLogicalShiftLeftOp>(dst, value, amount, setFlags);
    return dst;
}

Variable Emitter::LogicalShiftRight(VarOrImmArg value, VarOrImmArg amount, bool setFlags) {
    auto dst = Var();
    Write<IRLogicalShiftRightOp>(dst, value, amount, setFlags);
    return dst;
}

Variable Emitter::ArithmeticShiftRight(VarOrImmArg value, VarOrImmArg amount, bool setFlags) {
    auto dst = Var();
    Write<IRArithmeticShiftRightOp>(dst, value, amount, setFlags);
    return dst;
}

Variable Emitter::RotateRight(VarOrImmArg value, VarOrImmArg amount, bool setFlags) {
    auto dst = Var();
    Write<IRRotateRightOp>(dst, value, amount, setFlags);
    return dst;
}

Variable Emitter::RotateRightExtended(VarOrImmArg value, bool setFlags) {
    auto dst = Var();
    Write<IRRotateRightExtendedOp>(dst, value, setFlags);
    return dst;
}

Variable Emitter::BitwiseAnd(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    Write<IRBitwiseAndOp>(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::BitwiseOr(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    Write<IRBitwiseOrOp>(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::BitwiseXor(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    Write<IRBitwiseXorOp>(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::BitClear(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    Write<IRBitClearOp>(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::CountLeadingZeros(VarOrImmArg value) {
    auto dst = Var();
    Write<IRCountLeadingZerosOp>(dst, value);
    return dst;
}

Variable Emitter::Add(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    Write<IRAddOp>(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::AddCarry(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    Write<IRAddCarryOp>(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::Subtract(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    Write<IRSubtractOp>(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::SubtractCarry(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    Write<IRSubtractCarryOp>(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::Move(VarOrImmArg value, bool setFlags) {
    auto dst = Var();
    Write<IRMoveOp>(dst, value, setFlags);
    return dst;
}

Variable Emitter::MoveNegated(VarOrImmArg value, bool setFlags) {
    auto dst = Var();
    Write<IRMoveNegatedOp>(dst, value, setFlags);
    return dst;
}

void Emitter::Test(VarOrImmArg lhs, VarOrImmArg rhs) {
    Write<IRBitwiseAndOp>(lhs, rhs);
}

void Emitter::TestEquivalence(VarOrImmArg lhs, VarOrImmArg rhs) {
    Write<IRBitwiseXorOp>(lhs, rhs);
}

void Emitter::Compare(VarOrImmArg lhs, VarOrImmArg rhs) {
    Write<IRSubtractOp>(lhs, rhs);
}

void Emitter::CompareNegated(VarOrImmArg lhs, VarOrImmArg rhs) {
    Write<IRAddOp>(lhs, rhs);
}

Variable Emitter::SaturatingAdd(VarOrImmArg lhs, VarOrImmArg rhs, bool setQ) {
    auto dst = Var();
    Write<IRSaturatingAddOp>(dst, lhs, rhs, setQ);
    return dst;
}

Variable Emitter::SaturatingSubtract(VarOrImmArg lhs, VarOrImmArg rhs, bool setQ) {
    auto dst = Var();
    Write<IRSaturatingSubtractOp>(dst, lhs, rhs, setQ);
    return dst;
}

Variable Emitter::Multiply(VarOrImmArg lhs, VarOrImmArg rhs, bool signedMul, bool setFlags) {
    auto dst = Var();
    Write<IRMultiplyOp>(dst, lhs, rhs, signedMul, setFlags);
    return dst;
}

ALUVarPair Emitter::MultiplyLong(VarOrImmArg lhs, VarOrImmArg rhs, bool signedMul, bool shiftDownHalf, bool setFlags) {
    auto dstLo = Var();
    auto dstHi = Var();
    Write<IRMultiplyLongOp>(dstLo, dstHi, lhs, rhs, signedMul, shiftDownHalf, setFlags);
    return {dstLo, dstHi};
}

ALUVarPair Emitter::AddLong(VarOrImmArg lhsLo, VarOrImmArg lhsHi, VarOrImmArg rhsLo, VarOrImmArg rhsHi, bool setFlags) {
    auto dstLo = Var();
    auto dstHi = Var();
    Write<IRAddLongOp>(dstLo, dstHi, lhsLo, lhsHi, rhsLo, rhsHi, setFlags);
    return {dstLo, dstHi};
}

void Emitter::StoreFlags(arm::Flags flags, VarOrImmArg values) {
    Write<IRStoreFlagsOp>(flags, values);
}

void Emitter::StoreFlags(arm::Flags flags, arm::Flags values) {
    StoreFlags(flags, static_cast<uint32_t>(values));
}

void Emitter::LoadFlags(arm::Flags flags) {
    auto srcCPSR = GetCPSR();
    auto dstCPSR = Var();
    Write<IRLoadFlagsOp>(flags, dstCPSR, srcCPSR);
    SetCPSR(dstCPSR);
}

void Emitter::LoadStickyOverflow() {
    auto srcCPSR = GetCPSR();
    auto dstCPSR = Var();
    Write<IRLoadStickyOverflowOp>(dstCPSR, srcCPSR);
    SetCPSR(dstCPSR);
}

arm::Flags Emitter::SetNZ(arm::Flags mask, uint32_t value) {
    arm::Flags flags = arm::Flags::None;
    auto bmMask = BitmaskEnum(mask);
    if (bmMask.AnyOf(arm::Flags::N) && value >> 31) {
        flags |= arm::Flags::N;
    }
    if (bmMask.AnyOf(arm::Flags::Z) && value == 0) {
        flags |= arm::Flags::Z;
    }
    StoreFlags(arm::Flags::NZ, static_cast<uint32_t>(flags));
    return flags;
}

arm::Flags Emitter::SetNZ(arm::Flags mask, uint64_t value) {
    arm::Flags flags = arm::Flags::None;
    auto bmMask = BitmaskEnum(mask);
    if (bmMask.AnyOf(arm::Flags::N) && value >> 63ull) {
        flags |= arm::Flags::N;
    }
    if (bmMask.AnyOf(arm::Flags::Z) && value == 0) {
        flags |= arm::Flags::Z;
    }
    StoreFlags(arm::Flags::NZ, static_cast<uint32_t>(flags));
    return flags;
}

arm::Flags Emitter::SetNZCV(arm::Flags mask, uint32_t value, bool carry, bool overflow) {
    arm::Flags flags = arm::Flags::None;
    auto bmMask = BitmaskEnum(mask);
    if (bmMask.AnyOf(arm::Flags::N) && value >> 31) {
        flags |= arm::Flags::N;
    }
    if (bmMask.AnyOf(arm::Flags::Z) && value == 0) {
        flags |= arm::Flags::Z;
    }
    if (bmMask.AnyOf(arm::Flags::C) && carry) {
        flags |= arm::Flags::C;
    }
    if (bmMask.AnyOf(arm::Flags::V) && overflow) {
        flags |= arm::Flags::V;
    }
    StoreFlags(arm::Flags::NZCV, static_cast<uint32_t>(flags));
    return flags;
}

void Emitter::Branch(VarOrImmArg address) {
    Write<IRBranchOp>(address);
}

void Emitter::BranchExchange(VarOrImmArg address) {
    Write<IRBranchExchangeOp>(address);
}

Variable Emitter::LoadCopRegister(uint8_t cpnum, uint8_t opcode1, uint8_t crn, uint8_t crm, uint8_t opcode2, bool ext) {
    auto dstValue = Var();
    Write<IRLoadCopRegisterOp>(dstValue, cpnum, opcode1, crn, crm, opcode2, ext);
    return dstValue;
}

void Emitter::StoreCopRegister(uint8_t cpnum, uint8_t opcode1, uint8_t crn, uint8_t crm, uint8_t opcode2, bool ext,
                               VarOrImmArg srcValue) {
    Write<IRStoreCopRegisterOp>(srcValue, cpnum, opcode1, crn, crm, opcode2, ext);
}

void Emitter::Constant(VariableArg dst, uint32_t value) {
    Write<IRConstantOp>(dst, value);
}

Variable Emitter::Constant(uint32_t value) {
    auto dst = Var();
    Constant(dst, value);
    return dst;
}

void Emitter::CopyVar(VariableArg dst, VariableArg var) {
    Write<IRCopyVarOp>(dst, var);
}

Variable Emitter::CopyVar(VariableArg var) {
    auto dst = Var();
    Write<IRCopyVarOp>(dst, var);
    return dst;
}

Variable Emitter::GetBaseVectorAddress() {
    auto dst = Var();
    Write<IRGetBaseVectorAddressOp>(dst);
    return dst;
}

Variable Emitter::GetOffsetFromCurrentInstructionAddress(int32_t offset) {
    auto pc = GetRegister(arm::GPR::PC);
    return Add(pc, offset - m_instrSize * 2, false);
}

void Emitter::CopySPSRToCPSR() {
    auto spsr = GetSPSR();
    SetCPSR(spsr);
}

Variable Emitter::ComputeAddress(const arm::Addressing &addressing) {
    auto baseReg = GetRegister({addressing.baseReg, m_mode});
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
    auto value = GetRegister({shift.srcReg, m_mode});

    VarOrImmArg amount;
    if (shift.immediate) {
        amount = shift.amount.imm;
    } else {
        amount = GetRegister({shift.amount.reg, m_mode});
        // TODO: add one I cycle
    }

    Variable result{};
    switch (shift.type) {
    case arm::ShiftType::LSL:
        if (shift.immediate && shift.amount.imm == 0) {
            result = value;
            setFlags = false;
        } else {
            result = LogicalShiftLeft(value, amount, setFlags);
        }
        break;
    case arm::ShiftType::LSR: result = LogicalShiftRight(value, amount, setFlags); break;
    case arm::ShiftType::ASR: result = ArithmeticShiftRight(value, amount, setFlags); break;
    case arm::ShiftType::ROR:
        if (shift.immediate && shift.amount.imm == 0) {
            result = RotateRightExtended(value, setFlags);
        } else {
            result = RotateRight(value, amount, setFlags);
        }
        break;
    }

    if (setFlags) {
        LoadFlags(arm::Flags::C);
    }
    return result;
}

void Emitter::LinkBeforeBranch() {
    auto linkAddress = GetOffsetFromCurrentInstructionAddress(m_instrSize);
    if (m_thumb) {
        linkAddress = BitwiseOr(linkAddress, 1, false);
    }
    SetRegister({arm::GPR::LR, m_mode}, linkAddress);
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

    auto lr = GetOffsetFromCurrentInstructionAddress(nn);
    auto pc = Add(GetBaseVectorAddress(), static_cast<uint32_t>(vector) * 4 + sizeof(uint32_t) * 2, false);
    SetRegister({arm::GPR::LR, m_mode}, lr);
    SetRegister({arm::GPR::PC, m_mode}, pc);
}

void Emitter::FetchInstruction() {
    auto pc = GetRegister(arm::GPR::PC);
    pc = Add(pc, m_instrSize, false);
    SetRegister(arm::GPR::PC, pc);
}

Variable Emitter::Var() {
    return Variable{m_block.NextVarID()};
}

} // namespace armajitto::ir
