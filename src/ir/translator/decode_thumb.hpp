#pragma once

#include "armajitto/defs/arm/instructions.hpp"
#include "armajitto/util/bit_ops.hpp"

namespace armajitto::arm::thumb_decoder {

namespace detail {

    inline auto SimpleRegShift(GPR reg) {
        arm::RegisterSpecifiedShift shift{};
        shift.type = arm::ShiftType::LSL;
        shift.immediate = true;
        shift.srcReg = reg;
        shift.amount.imm = 0;
        return shift;
    }

} // namespace detail

inline auto ShiftByImm(uint16_t opcode) {
    arm::instrs::DataProcessing instr{};

    instr.opcode = arm::instrs::DataProcessing::Opcode::MOV;
    instr.immediate = false;
    instr.setFlags = true;
    instr.dstReg = static_cast<GPR>(bit::extract<0, 3>(opcode));
    instr.lhsReg = instr.dstReg;

    const uint8_t op = bit::extract<11, 2>(opcode);
    switch (op) {
    case 0b00: instr.rhs.shift.type = arm::ShiftType::LSL; break;
    case 0b01: instr.rhs.shift.type = arm::ShiftType::LSR; break;
    case 0b10: instr.rhs.shift.type = arm::ShiftType::ASR; break;
    default: break; // TODO: unreachable
    }
    instr.rhs.shift.immediate = true;
    instr.rhs.shift.srcReg = static_cast<GPR>(bit::extract<3, 3>(opcode));
    instr.rhs.shift.amount.imm = bit::extract<6, 5>(opcode);

    return instr;
}

inline auto AddSubRegImm(uint16_t opcode) {
    arm::instrs::DataProcessing instr{};

    if (bit::test<9>(opcode)) {
        instr.opcode = arm::instrs::DataProcessing::Opcode::SUB;
    } else {
        instr.opcode = arm::instrs::DataProcessing::Opcode::ADD;
    }
    instr.immediate = bit::test<10>(opcode);
    instr.setFlags = true;
    instr.dstReg = static_cast<GPR>(bit::extract<0, 3>(opcode));
    instr.lhsReg = static_cast<GPR>(bit::extract<3, 3>(opcode));
    if (instr.immediate) {
        instr.rhs.imm = bit::extract<6, 3>(opcode);
    } else {
        instr.rhs.shift = detail::SimpleRegShift(static_cast<GPR>(bit::extract<6, 3>(opcode)));
    }

    return instr;
}

inline auto MovCmpAddSubImm(uint16_t opcode) {
    arm::instrs::DataProcessing instr{};

    switch (bit::extract<11, 2>(opcode)) {
    case 0b00: instr.opcode = arm::instrs::DataProcessing::Opcode::MOV;
    case 0b01: instr.opcode = arm::instrs::DataProcessing::Opcode::CMP;
    case 0b10: instr.opcode = arm::instrs::DataProcessing::Opcode::ADD;
    case 0b11: instr.opcode = arm::instrs::DataProcessing::Opcode::SUB;
    }
    instr.immediate = true;
    instr.setFlags = true;
    instr.dstReg = static_cast<GPR>(bit::extract<8, 3>(opcode));
    instr.lhsReg = instr.dstReg;
    instr.rhs.imm = bit::extract<0, 8>(opcode);

    return instr;
}

inline auto DataProcessingStandard(uint16_t opcode, arm::instrs::DataProcessing::Opcode dpOpcode) {
    arm::instrs::DataProcessing instr{};

    instr.opcode = dpOpcode;
    instr.immediate = false;
    instr.setFlags = true;
    instr.dstReg = static_cast<GPR>(bit::extract<12, 3>(opcode));
    instr.lhsReg = instr.dstReg;
    instr.rhs.shift = detail::SimpleRegShift(static_cast<GPR>(bit::extract<0, 3>(opcode)));

    return instr;
}

inline auto DataProcessingShift(uint16_t opcode, arm::ShiftType shiftType) {
    arm::instrs::DataProcessing instr{};

    instr.opcode = arm::instrs::DataProcessing::Opcode::MOV;
    instr.immediate = false;
    instr.setFlags = true;
    instr.dstReg = static_cast<GPR>(bit::extract<0, 3>(opcode));
    instr.lhsReg = GPR::R0;
    instr.rhs.shift.type = shiftType;
    instr.rhs.shift.immediate = false;
    instr.rhs.shift.srcReg = instr.dstReg;
    instr.rhs.shift.amount.reg = static_cast<GPR>(bit::extract<3, 3>(opcode));

    return instr;
}

inline auto DataProcessingNegate(uint16_t opcode) {
    arm::instrs::DataProcessing instr{};

    instr.opcode = arm::instrs::DataProcessing::Opcode::RSB;
    instr.immediate = true;
    instr.setFlags = true;
    instr.dstReg = static_cast<GPR>(bit::extract<12, 3>(opcode));
    instr.lhsReg = instr.dstReg;
    instr.rhs.imm = 0;

    return instr;
}

inline auto DataProcessingMultiply(uint16_t opcode) {
    arm::instrs::MultiplyAccumulate instr{};

    instr.dstReg = static_cast<GPR>(bit::extract<0, 3>(opcode));
    instr.lhsReg = instr.dstReg;
    instr.rhsReg = static_cast<GPR>(bit::extract<3, 3>(opcode));
    instr.accReg = GPR::R0;
    instr.accumulate = false;
    instr.setFlags = true;

    return instr;
}

inline auto HiRegOps(uint16_t opcode) {
    arm::instrs::DataProcessing instr{};

    const uint8_t h1 = bit::extract<7>(opcode);
    const uint8_t h2 = bit::extract<6>(opcode);
    switch (bit::extract<8, 2>(opcode)) {
    case 0b00: instr.opcode = arm::instrs::DataProcessing::Opcode::ADD; break;
    case 0b01: instr.opcode = arm::instrs::DataProcessing::Opcode::CMP; break;
    case 0b10: instr.opcode = arm::instrs::DataProcessing::Opcode::MOV; break;
    }
    instr.immediate = false;
    instr.setFlags = false;
    instr.dstReg = static_cast<GPR>(bit::extract<0, 3>(opcode) + h1 * 8);
    instr.lhsReg = GPR::R0;
    instr.rhs.shift = detail::SimpleRegShift(static_cast<GPR>(bit::extract<3, 3>(opcode) + h2 * 8));

    return instr;
}

inline auto HiRegBranchExchange(uint16_t opcode, bool link) {
    arm::instrs::BranchExchangeRegister instr{};

    const uint8_t h2 = bit::extract<6>(opcode);
    instr.reg = static_cast<GPR>(bit::extract<3, 3>(opcode) + h2 * 8);
    instr.link = link;

    return instr;
}

inline auto PCRelativeLoad(uint16_t opcode) {
    arm::instrs::SingleDataTransfer instr{};

    instr.preindexed = true;
    instr.byte = false;
    instr.writeback = false;
    instr.load = true;
    instr.reg = static_cast<GPR>(bit::extract<8, 3>(opcode));
    instr.address.immediate = true;
    instr.address.positiveOffset = true;
    instr.address.baseReg = GPR::PC;
    instr.address.immValue = bit::extract<0, 8>(opcode) * 4;

    return instr;
}

inline auto LoadStoreByteWordRegOffset(uint16_t opcode) {
    arm::instrs::SingleDataTransfer instr{};

    instr.preindexed = true;
    instr.byte = bit::test<10>(opcode);
    instr.writeback = false;
    instr.load = bit::test<11>(opcode);
    instr.reg = static_cast<GPR>(bit::extract<0, 3>(opcode));
    instr.address.immediate = false;
    instr.address.positiveOffset = true;
    instr.address.baseReg = static_cast<GPR>(bit::extract<3, 3>(opcode));
    instr.address.shift = detail::SimpleRegShift(static_cast<GPR>(bit::extract<6, 3>(opcode)));

    return instr;
}

inline auto LoadStoreHalfRegOffset(uint16_t opcode) {
    arm::instrs::HalfwordAndSignedTransfer instr{};

    instr.preindexed = true;
    instr.positiveOffset = true;
    instr.immediate = false;
    instr.writeback = false;

    //              load sign half
    // 00 = STRH      -    -    +
    // 01 = LDRSB     +    +    -
    // 10 = LDRH      +    -    +
    // 11 = LDRSH     +    +    +
    const uint8_t op = bit::extract<10, 2>(opcode);
    instr.load = (op != 0b00);
    instr.sign = bit::test<0>(op);
    instr.half = (op != 0b01);

    instr.reg = static_cast<GPR>(bit::extract<0, 3>(opcode));
    instr.baseReg = static_cast<GPR>(bit::extract<3, 3>(opcode));
    instr.offset.reg = static_cast<GPR>(bit::extract<6, 3>(opcode));

    return instr;
}

inline auto LoadStoreByteWordImmOffset(uint16_t opcode) {
    arm::instrs::SingleDataTransfer instr{};

    instr.preindexed = true;
    instr.byte = bit::test<12>(opcode);
    instr.writeback = false;
    instr.load = bit::test<11>(opcode);
    instr.reg = static_cast<GPR>(bit::extract<0, 3>(opcode));
    instr.address.immediate = true;
    instr.address.positiveOffset = true;
    instr.address.baseReg = static_cast<GPR>(bit::extract<3, 3>(opcode));
    instr.address.immValue = bit::extract<6, 5>(opcode);

    return instr;
}

inline auto LoadStoreHalfImmOffset(uint16_t opcode) {
    arm::instrs::HalfwordAndSignedTransfer instr{};

    instr.preindexed = true;
    instr.positiveOffset = true;
    instr.immediate = true;
    instr.writeback = false;
    instr.load = bit::test<11>(opcode);
    instr.sign = false;
    instr.half = true;
    instr.reg = static_cast<GPR>(bit::extract<0, 3>(opcode));
    instr.baseReg = static_cast<GPR>(bit::extract<3, 3>(opcode));
    instr.offset.imm = bit::extract<6, 5>(opcode);

    return instr;
}

inline auto SPRelativeLoadStore(uint16_t opcode) {
    arm::instrs::SingleDataTransfer instr{};

    instr.preindexed = true;
    instr.byte = false;
    instr.writeback = false;
    instr.load = bit::test<11>(opcode);
    instr.reg = static_cast<GPR>(bit::extract<8, 3>(opcode));
    instr.address.immediate = true;
    instr.address.positiveOffset = true;
    instr.address.baseReg = GPR::SP;
    instr.address.immValue = bit::extract<0, 8>(opcode) * 4;

    return instr;
}

inline auto AddToSPOrPC(uint16_t opcode) {
    arm::instrs::DataProcessing instr{};

    instr.opcode = arm::instrs::DataProcessing::Opcode::ADD;
    instr.immediate = true;
    instr.setFlags = false;
    instr.dstReg = static_cast<GPR>(bit::extract<8, 3>(opcode));
    instr.lhsReg = bit::test<11>(opcode) ? GPR::SP : GPR::PC;
    instr.rhs.imm = bit::extract<0, 8>(opcode) * 4;

    return instr;
}

inline auto AdjustSP(uint16_t opcode) {
    arm::instrs::DataProcessing instr{};

    if (bit::test<7>(opcode)) {
        instr.opcode = arm::instrs::DataProcessing::Opcode::SUB;
    } else {
        instr.opcode = arm::instrs::DataProcessing::Opcode::ADD;
    }
    instr.immediate = true;
    instr.setFlags = false;
    instr.dstReg = GPR::SP;
    instr.lhsReg = GPR::SP;
    instr.rhs.imm = bit::extract<0, 7>(opcode) * 4;

    return instr;
}

inline auto PushPop(uint16_t opcode) {
    arm::instrs::BlockTransfer instr{};

    //                   P U S W L   reg included by R bit
    // PUSH = STMDB sp!  + - - + -   LR
    // POP  = LDMIA sp!  - + - + +   PC
    const bool load = bit::test<11>(opcode);
    instr.preindexed = !load;
    instr.positiveOffset = load;
    instr.userModeOrPSRTransfer = false;
    instr.writeback = true;
    instr.load = load;
    instr.baseReg = GPR::SP;
    instr.regList = bit::extract<0, 8>(opcode);
    if (bit::test<8>(opcode)) {
        if (load) {
            instr.regList |= (1 << 15);
        } else {
            instr.regList |= (1 << 14);
        }
    }

    return instr;
}

inline auto LoadStoreMultiple(uint16_t opcode) {
    arm::instrs::BlockTransfer instr{};

    // load  P U S W L
    //   -   - + - + -
    //   +   - + - * +   *: true if rn not in list, false otherwise
    const bool load = bit::test<11>(opcode);
    const uint8_t regList = bit::extract<0, 8>(opcode);
    const uint8_t rn = bit::extract<8, 3>(opcode);
    instr.preindexed = false;
    instr.positiveOffset = true;
    instr.userModeOrPSRTransfer = false;
    instr.writeback = !load || !bit::test(rn, regList);
    instr.load = load;
    instr.baseReg = GPR::SP;
    instr.regList = regList;

    return instr;
}

inline auto SoftwareInterrupt(uint16_t opcode) {
    return arm::instrs::SoftwareInterrupt{.comment = bit::extract<0, 8>(opcode)};
}

inline auto SoftwareBreakpoint() {
    return arm::instrs::SoftwareBreakpoint{};
}

inline auto ConditionalBranch(uint16_t opcode) {
    arm::instrs::BranchOffset instr{};

    instr.offset = bit::sign_extend<8, int32_t>(bit::extract<0, 8>(opcode)) * 2;
    instr.type = arm::instrs::BranchOffset::Type::B;

    return instr;
}

inline auto UnconditionalBranch(uint16_t opcode) {
    arm::instrs::BranchOffset instr{};

    instr.offset = bit::sign_extend<11, int32_t>(bit::extract<0, 11>(opcode)) * 2;
    instr.type = arm::instrs::BranchOffset::Type::B;

    return instr;
}

inline auto LongBranchPrefix(uint16_t opcode) {
    arm::instrs::DataProcessing instr{};

    // LR = PC + (SignExtend(offset_11) << 12)
    instr.opcode = arm::instrs::DataProcessing::Opcode::ADD;
    instr.immediate = true;
    instr.setFlags = false;
    instr.dstReg = GPR::LR;
    instr.lhsReg = GPR::PC;
    instr.rhs.imm = bit::sign_extend<11>(bit::extract<0, 11>(opcode)) << 12;

    return instr;
}

inline auto LongBranchSuffix(uint16_t opcode, bool blx) {
    arm::instrs::ThumbLongBranchSuffix instr{};

    instr.offset = bit::extract<0, 11>(opcode) * 2;
    instr.blx = blx;

    return instr;
}

inline auto Undefined() {
    return arm::instrs::Undefined{};
}

} // namespace armajitto::arm::thumb_decoder
