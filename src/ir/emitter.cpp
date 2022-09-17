#include "emitter.hpp"
#include "ir_ops.hpp"

#include "guest/arm/exception_vectors.hpp"

#include "util/bit_ops.hpp"

namespace armajitto::ir {

void Emitter::NextInstruction() {
    m_block.NextInstruction();
}

void Emitter::SetCondition(arm::Condition cond) {
    m_block.SetCondition(cond);
}

// ---------------------------------------------------------------------------------------------------------------------
// Basic IR instruction emitters

Variable Emitter::GetRegister(GPRArg src) {
    auto dst = Var();
    GetRegister(dst, src);
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
    GetCPSR(dst);
    return dst;
}

void Emitter::SetCPSR(VarOrImmArg src, bool updateIFlag) {
    Write<IRSetCPSROp>(src, updateIFlag);
}

Variable Emitter::GetSPSR() {
    auto dst = Var();
    GetSPSR(dst);
    return dst;
}

void Emitter::SetSPSR(VarOrImmArg src) {
    SetSPSR(src, m_mode);
}

void Emitter::SetSPSR(VarOrImmArg src, arm::Mode mode) {
    // Emit only if the target mode has an SPSR register
    if (arm::NormalizedIndex(mode) != 0) {
        Write<IRSetSPSROp>(mode, src);
    }
}

Variable Emitter::MemRead(MemAccessBus bus, MemAccessMode mode, MemAccessSize size, VarOrImmArg address) {
    auto dst = Var();
    MemRead(bus, mode, size, dst, address);
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
    LogicalShiftLeft(dst, value, amount, setFlags);
    return dst;
}

Variable Emitter::LogicalShiftRight(VarOrImmArg value, VarOrImmArg amount, bool setFlags) {
    auto dst = Var();
    LogicalShiftRight(dst, value, amount, setFlags);
    return dst;
}

Variable Emitter::ArithmeticShiftRight(VarOrImmArg value, VarOrImmArg amount, bool setFlags) {
    auto dst = Var();
    ArithmeticShiftRight(dst, value, amount, setFlags);
    return dst;
}

Variable Emitter::RotateRight(VarOrImmArg value, VarOrImmArg amount, bool setFlags) {
    auto dst = Var();
    RotateRight(dst, value, amount, setFlags);
    return dst;
}

Variable Emitter::RotateRightExtended(VarOrImmArg value, bool setFlags) {
    auto dst = Var();
    RotateRightExtended(dst, value, setFlags);
    return dst;
}

Variable Emitter::BitwiseAnd(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    BitwiseAnd(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::BitwiseOr(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    BitwiseOr(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::BitwiseXor(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    BitwiseXor(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::BitClear(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    BitClear(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::CountLeadingZeros(VarOrImmArg value) {
    auto dst = Var();
    CountLeadingZeros(dst, value);
    return dst;
}

Variable Emitter::Add(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    Add(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::AddCarry(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    AddCarry(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::Subtract(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    Subtract(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::SubtractCarry(VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    auto dst = Var();
    SubtractCarry(dst, lhs, rhs, setFlags);
    return dst;
}

Variable Emitter::Move(VarOrImmArg value, bool setFlags) {
    auto dst = Var();
    Move(dst, value, setFlags);
    return dst;
}

Variable Emitter::MoveNegated(VarOrImmArg value, bool setFlags) {
    auto dst = Var();
    MoveNegated(dst, value, setFlags);
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
    SaturatingAdd(dst, lhs, rhs, setQ);
    return dst;
}

Variable Emitter::SaturatingSubtract(VarOrImmArg lhs, VarOrImmArg rhs, bool setQ) {
    auto dst = Var();
    SaturatingSubtract(dst, lhs, rhs, setQ);
    return dst;
}

Variable Emitter::Multiply(VarOrImmArg lhs, VarOrImmArg rhs, bool signedMul, bool setFlags) {
    auto dst = Var();
    Multiply(dst, lhs, rhs, signedMul, setFlags);
    return dst;
}

ALUVarPair Emitter::MultiplyLong(VarOrImmArg lhs, VarOrImmArg rhs, bool signedMul, bool shiftDownHalf, bool setFlags) {
    auto dstLo = Var();
    auto dstHi = Var();
    MultiplyLong(dstLo, dstHi, lhs, rhs, signedMul, shiftDownHalf, setFlags);
    return {dstLo, dstHi};
}

ALUVarPair Emitter::AddLong(VarOrImmArg lhsLo, VarOrImmArg lhsHi, VarOrImmArg rhsLo, VarOrImmArg rhsHi, bool setFlags) {
    auto dstLo = Var();
    auto dstHi = Var();
    AddLong(dstLo, dstHi, lhsLo, lhsHi, rhsLo, rhsHi, setFlags);
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
    SetCPSR(dstCPSR, false);
}

void Emitter::LoadStickyOverflow() {
    auto srcCPSR = GetCPSR();
    auto dstCPSR = Var();
    Write<IRLoadStickyOverflowOp>(dstCPSR, srcCPSR);
    SetCPSR(dstCPSR, false);
}

void Emitter::SetC(bool value) {
    arm::Flags flags = value ? arm::Flags::C : arm::Flags::None;
    StoreFlags(arm::Flags::C, static_cast<uint32_t>(flags));
}

arm::Flags Emitter::SetNZ(uint32_t value) {
    arm::Flags flags = arm::Flags::None;
    if (value >> 31) {
        flags |= arm::Flags::N;
    }
    if (value == 0) {
        flags |= arm::Flags::Z;
    }
    StoreFlags(arm::Flags::NZ, static_cast<uint32_t>(flags));
    return flags;
}

arm::Flags Emitter::SetNZ(uint64_t value) {
    arm::Flags flags = arm::Flags::None;
    if (value >> 63ull) {
        flags |= arm::Flags::N;
    }
    if (value == 0) {
        flags |= arm::Flags::Z;
    }
    StoreFlags(arm::Flags::NZ, static_cast<uint32_t>(flags));
    return flags;
}

arm::Flags Emitter::SetNZCV(uint32_t value, bool carry, bool overflow) {
    arm::Flags flags = arm::Flags::None;
    if (value >> 31) {
        flags |= arm::Flags::N;
    }
    if (value == 0) {
        flags |= arm::Flags::Z;
    }
    if (carry) {
        flags |= arm::Flags::C;
    }
    if (overflow) {
        flags |= arm::Flags::V;
    }
    StoreFlags(arm::Flags::NZCV, static_cast<uint32_t>(flags));
    return flags;
}

void Emitter::Branch(VarOrImmArg address) {
    Write<IRBranchOp>(address);

    if (address.immediate) {
        auto loc = m_block.Location();
        TerminateDirectLink(address.imm.value, loc.Mode(), loc.IsThumbMode());
    } else {
        TerminateIndirectLink();
    }
}

void Emitter::BranchExchange(VarOrImmArg address) {
    Write<IRBranchExchangeOp>(address);

    if (address.immediate) {
        auto loc = m_block.Location();
        TerminateDirectLink(address.imm.value, loc.Mode(), bit::test<0>(address.imm.value));
    } else {
        TerminateIndirectLink();
    }
}

void Emitter::BranchExchangeL4(VarOrImmArg address) {
    Write<IRBranchExchangeOp>(address, IRBranchExchangeOp::ExchangeMode::L4);
    TerminateIndirectLink();
}

void Emitter::BranchExchangeCPSRThumbFlag(VarOrImmArg address) {
    Write<IRBranchExchangeOp>(address, IRBranchExchangeOp::ExchangeMode::CPSRThumbFlag);
    TerminateIndirectLink();
}

Variable Emitter::LoadCopRegister(uint8_t cpnum, arm::CopRegister reg, bool ext) {
    auto dstValue = Var();
    LoadCopRegister(dstValue, cpnum, reg, ext);
    return dstValue;
}

void Emitter::StoreCopRegister(uint8_t cpnum, arm::CopRegister reg, bool ext, VarOrImmArg srcValue) {
    Write<IRStoreCopRegisterOp>(srcValue, cpnum, reg, ext);
}

Variable Emitter::Constant(uint32_t value) {
    auto dst = Var();
    Constant(dst, value);
    return dst;
}

Variable Emitter::CopyVar(VariableArg var) {
    auto dst = Var();
    CopyVar(dst, var);
    return dst;
}

Variable Emitter::GetBaseVectorAddress() {
    auto dst = Var();
    GetBaseVectorAddress(dst);
    return dst;
}

// ---------------------------------------------------------------------------------------------------------------------
// Basic IR instruction emitters with destination variables

void Emitter::GetRegister(VariableArg dst, GPRArg src) {
    Write<IRGetRegisterOp>(dst, src);
}

void Emitter::GetRegister(VariableArg dst, arm::GPR src) {
    GetRegister(dst, {src, m_mode});
}

void Emitter::GetCPSR(VariableArg dst) {
    Write<IRGetCPSROp>(dst);
}

void Emitter::GetSPSR(VariableArg dst) {
    Write<IRGetSPSROp>(dst, m_mode);
}

void Emitter::MemRead(MemAccessBus bus, MemAccessMode mode, MemAccessSize size, VariableArg dst, VarOrImmArg address) {
    Write<IRMemReadOp>(bus, mode, size, dst, address);
}

void Emitter::LogicalShiftLeft(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setFlags) {
    Write<IRLogicalShiftLeftOp>(dst, value, amount, setFlags);
}

void Emitter::LogicalShiftRight(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setFlags) {
    Write<IRLogicalShiftRightOp>(dst, value, amount, setFlags);
}

void Emitter::ArithmeticShiftRight(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setFlags) {
    Write<IRArithmeticShiftRightOp>(dst, value, amount, setFlags);
}

void Emitter::RotateRight(VariableArg dst, VarOrImmArg value, VarOrImmArg amount, bool setFlags) {
    Write<IRRotateRightOp>(dst, value, amount, setFlags);
}

void Emitter::RotateRightExtended(VariableArg dst, VarOrImmArg value, bool setFlags) {
    Write<IRRotateRightExtendedOp>(dst, value, setFlags);
}

void Emitter::BitwiseAnd(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    Write<IRBitwiseAndOp>(dst, lhs, rhs, setFlags);
}

void Emitter::BitwiseOr(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    Write<IRBitwiseOrOp>(dst, lhs, rhs, setFlags);
}

void Emitter::BitwiseXor(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    Write<IRBitwiseXorOp>(dst, lhs, rhs, setFlags);
}

void Emitter::BitClear(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    Write<IRBitClearOp>(dst, lhs, rhs, setFlags);
}

void Emitter::CountLeadingZeros(VariableArg dst, VarOrImmArg value) {
    Write<IRCountLeadingZerosOp>(dst, value);
}

void Emitter::Add(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    Write<IRAddOp>(dst, lhs, rhs, setFlags);
}

void Emitter::AddCarry(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    Write<IRAddCarryOp>(dst, lhs, rhs, setFlags);
}

void Emitter::Subtract(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    Write<IRSubtractOp>(dst, lhs, rhs, setFlags);
}

void Emitter::SubtractCarry(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setFlags) {
    Write<IRSubtractCarryOp>(dst, lhs, rhs, setFlags);
}

void Emitter::Move(VariableArg dst, VarOrImmArg value, bool setFlags) {
    Write<IRMoveOp>(dst, value, setFlags);
}

void Emitter::MoveNegated(VariableArg dst, VarOrImmArg value, bool setFlags) {
    Write<IRMoveNegatedOp>(dst, value, setFlags);
}

void Emitter::SaturatingAdd(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setQ) {
    Write<IRSaturatingAddOp>(dst, lhs, rhs, setQ);
}

void Emitter::SaturatingSubtract(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool setQ) {
    Write<IRSaturatingSubtractOp>(dst, lhs, rhs, setQ);
}

void Emitter::Multiply(VariableArg dst, VarOrImmArg lhs, VarOrImmArg rhs, bool signedMul, bool setFlags) {
    Write<IRMultiplyOp>(dst, lhs, rhs, signedMul, setFlags);
}

void Emitter::MultiplyLong(VariableArg dstLo, VariableArg dstHi, VarOrImmArg lhs, VarOrImmArg rhs, bool signedMul,
                           bool shiftDownHalf, bool setFlags) {
    Write<IRMultiplyLongOp>(dstLo, dstHi, lhs, rhs, signedMul, shiftDownHalf, setFlags);
}

void Emitter::AddLong(VariableArg dstLo, VariableArg dstHi, VarOrImmArg lhsLo, VarOrImmArg lhsHi, VarOrImmArg rhsLo,
                      VarOrImmArg rhsHi, bool setFlags) {
    Write<IRAddLongOp>(dstLo, dstHi, lhsLo, lhsHi, rhsLo, rhsHi, setFlags);
}

void Emitter::LoadCopRegister(VariableArg dstValue, uint8_t cpnum, arm::CopRegister reg, bool ext) {
    Write<IRLoadCopRegisterOp>(dstValue, cpnum, reg, ext);
}

void Emitter::Constant(VariableArg dst, uint32_t value) {
    Write<IRConstantOp>(dst, value);
}

void Emitter::CopyVar(VariableArg dst, VariableArg var) {
    Write<IRCopyVarOp>(dst, var);
}

void Emitter::GetBaseVectorAddress(VariableArg dst) {
    Write<IRGetBaseVectorAddressOp>(dst);
}

// ---------------------------------------------------------------------------------------------------------------------
// Complex IR instruction sequence emitters

Variable Emitter::AddQ(VarOrImmArg lhs, VarOrImmArg rhs) {
    auto dst = Var();
    Write<IRAddOp>(dst, lhs, rhs, arm::Flags::V);
    return dst;
}

Variable Emitter::GetOffsetFromCurrentInstructionAddress(int32_t offset) {
    auto pc = GetRegister(arm::GPR::PC);
    return Add(pc, offset - m_instrSize * 2, false);
}

void Emitter::CopySPSRToCPSR() {
    auto spsr = GetSPSR();
    StoreFlags(arm::Flags::NZCV, spsr);
    SetCPSR(spsr, true);
    TerminateReturn();
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
        if (shift.amount.reg == arm::GPR::PC) {
            // Compensate for pipeline effects
            amount = Subtract(amount, m_instrSize, false);
        }
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
    SetCPSR(cpsr, true);

    auto lr = GetOffsetFromCurrentInstructionAddress(nn);
    auto pc = Add(GetBaseVectorAddress(), static_cast<uint32_t>(vector) * 4 + sizeof(uint32_t) * 2, false);
    SetRegister({arm::GPR::LR, vectorInfo.mode}, lr);
    SetRegister({arm::GPR::PC, vectorInfo.mode}, pc);

    TerminateReturn();
}

void Emitter::FetchInstruction() {
    auto pc = GetRegister(arm::GPR::PC);
    pc = Add(pc, m_instrSize, false);
    SetRegister(arm::GPR::PC, pc);
}

void Emitter::TerminateDirectLink(uint32_t targetAddress, arm::Mode mode, bool thumb) {
    const uint32_t instrSize = (thumb ? sizeof(uint16_t) : sizeof(uint32_t));
    const uint32_t addrMask = ~(instrSize - 1);
    const uint32_t pc = (targetAddress & addrMask) + 2 * instrSize;
    m_block.TerminateDirectLink({pc, mode, thumb});
}

void Emitter::TerminateIndirectLink() {
    m_block.TerminateIndirectLink();
}

void Emitter::TerminateContinueExecution() {
    m_block.TerminateDirectLinkNextBlock();
}

void Emitter::TerminateReturn() {
    m_block.TerminateReturn();
}

Variable Emitter::Var() {
    return Variable{m_block.NextVarID()};
}

} // namespace armajitto::ir
