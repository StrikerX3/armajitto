#pragma once

#include "guest/arm/instructions.hpp"

#include "util/bit_ops.hpp"

namespace armajitto::arm::arm_decoder {

namespace detail {
    inline std::pair<uint32_t, CarryResult> DecodeRotatedImm(uint32_t opcode) {
        const uint32_t imm = bit::extract<0, 8>(opcode);
        const uint32_t rotate = bit::extract<8, 4>(opcode) * 2;
        if (rotate == 0) {
            return {imm, CarryResult::NoChange};
        }
        bool carry = (imm >> (rotate - 1)) & 1;
        return {std::rotr(imm, rotate), (carry ? CarryResult::Set : CarryResult::Clear)};
    }

    inline auto DecodeShift(uint32_t opcode) {
        arm::RegisterSpecifiedShift shift{};
        const uint8_t shiftParam = bit::extract<4, 8>(opcode);
        shift.type = static_cast<arm::ShiftType>(bit::extract<1, 2>(shiftParam));
        shift.immediate = !bit::test<0>(shiftParam); // Note the inverted bit!
        shift.srcReg = static_cast<GPR>(bit::extract<0, 4>(opcode));
        if (shift.immediate) {
            shift.amount.imm = bit::extract<3, 5>(shiftParam);
            if ((shift.type == arm::ShiftType::LSR || shift.type == arm::ShiftType::ASR) && shift.amount.imm == 0) {
                shift.amount.imm = 32;
            }
        } else {
            shift.amount.reg = static_cast<GPR>(bit::extract<4, 4>(shiftParam));
        }
        return shift;
    }

    inline auto DecodeAddressing(uint32_t opcode) {
        arm::Addressing offset{};
        offset.immediate = !bit::test<25>(opcode); // Note the inverted bit!
        offset.positiveOffset = bit::test<23>(opcode);
        offset.baseReg = static_cast<GPR>(bit::extract<16, 4>(opcode));
        if (offset.immediate) {
            offset.immValue = bit::extract<0, 12>(opcode);
        } else {
            offset.shift = DecodeShift(opcode);
        }
        return offset;
    }
} // namespace detail

// B,BL,BLX (offset)
inline auto BranchOffset(uint32_t opcode, bool switchToThumb) {
    arm::instrs::BranchOffset instr{};

    using Type = arm::instrs::BranchOffset::Type;

    const bool bit24 = bit::test<24>(opcode);
    instr.offset = bit::sign_extend<24>(bit::extract<0, 24>(opcode)) << 2;
    if (switchToThumb) {
        instr.type = Type::BLX;
        instr.offset |= static_cast<int32_t>(bit24) << 1;
        instr.offset |= 1;
    } else {
        instr.type = bit24 ? Type::BL : Type::B; // L bit
    }

    return instr;
}

// BX,BLX (register)
inline auto BranchExchangeRegister(uint32_t opcode) {
    arm::instrs::BranchExchangeRegister instr{};

    instr.reg = static_cast<GPR>(bit::extract<0, 4>(opcode));
    instr.link = bit::test<5>(opcode);

    return instr;
}

// AND,EOR,SUB,RSB,ADD,ADC,SBC,RSC,TST,TEQ,CMP,CMN,ORR,MOV,BIC,MVN
inline auto DataProcessing(uint32_t opcode) {
    arm::instrs::DataProcessing instr{};

    instr.opcode = static_cast<arm::instrs::DataProcessing::Opcode>(bit::extract<21, 4>(opcode));
    instr.immediate = bit::test<25>(opcode);
    instr.setFlags = bit::test<20>(opcode);
    instr.dstReg = static_cast<GPR>(bit::extract<12, 4>(opcode));
    instr.lhsReg = static_cast<GPR>(bit::extract<16, 4>(opcode));
    if (instr.immediate) {
        auto [result, carry] = detail::DecodeRotatedImm(opcode);
        instr.rhs.imm.value = result;
        instr.rhs.imm.carry = carry;
    } else {
        instr.rhs.shift = detail::DecodeShift(opcode);
    }

    return instr;
}

// CLZ
inline auto CountLeadingZeros(uint32_t opcode) {
    arm::instrs::CountLeadingZeros instr{};

    instr.dstReg = static_cast<GPR>(bit::extract<12, 4>(opcode));
    instr.argReg = static_cast<GPR>(bit::extract<0, 4>(opcode));

    return instr;
}

// QADD,QSUB,QDADD,QDSUB
inline auto SaturatingAddSub(uint32_t opcode) {
    arm::instrs::SaturatingAddSub instr{};

    instr.dstReg = static_cast<GPR>(bit::extract<12, 4>(opcode));
    instr.lhsReg = static_cast<GPR>(bit::extract<0, 4>(opcode));
    instr.rhsReg = static_cast<GPR>(bit::extract<16, 4>(opcode));
    instr.sub = bit::test<21>(opcode);
    instr.dbl = bit::test<22>(opcode);

    return instr;
}

// MUL,MLA
inline auto MultiplyAccumulate(uint32_t opcode) {
    arm::instrs::MultiplyAccumulate instr{};

    instr.dstReg = static_cast<GPR>(bit::extract<16, 4>(opcode));
    instr.lhsReg = static_cast<GPR>(bit::extract<0, 4>(opcode));
    instr.rhsReg = static_cast<GPR>(bit::extract<8, 4>(opcode));
    instr.accReg = static_cast<GPR>(bit::extract<12, 4>(opcode));
    instr.accumulate = bit::test<21>(opcode);
    instr.setFlags = bit::test<20>(opcode);

    return instr;
}

// SMULL,UMULL,SMLAL,UMLAL
inline auto MultiplyAccumulateLong(uint32_t opcode) {
    arm::instrs::MultiplyAccumulateLong instr{};

    instr.dstAccLoReg = static_cast<GPR>(bit::extract<12, 4>(opcode));
    instr.dstAccHiReg = static_cast<GPR>(bit::extract<16, 4>(opcode));
    instr.lhsReg = static_cast<GPR>(bit::extract<0, 4>(opcode));
    instr.rhsReg = static_cast<GPR>(bit::extract<8, 4>(opcode));
    instr.signedMul = bit::test<22>(opcode);
    instr.accumulate = bit::test<21>(opcode);
    instr.setFlags = bit::test<20>(opcode);

    return instr;
}

// SMUL<x><y>,SMLA<x><y>
inline auto SignedMultiplyAccumulate(uint32_t opcode) {
    arm::instrs::SignedMultiplyAccumulate instr{};

    instr.dstReg = static_cast<GPR>(bit::extract<16, 4>(opcode));
    instr.lhsReg = static_cast<GPR>(bit::extract<0, 4>(opcode));
    instr.rhsReg = static_cast<GPR>(bit::extract<8, 4>(opcode));
    instr.accReg = static_cast<GPR>(bit::extract<12, 4>(opcode));
    instr.x = bit::test<5>(opcode);
    instr.y = bit::test<6>(opcode);
    instr.accumulate = !bit::test<21>(opcode); // Note the inverted bit!

    return instr;
}

// SMULW<y>,SMLAW<y>
inline auto SignedMultiplyAccumulateWord(uint32_t opcode) {
    arm::instrs::SignedMultiplyAccumulateWord instr{};

    instr.dstReg = static_cast<GPR>(bit::extract<16, 4>(opcode));
    instr.lhsReg = static_cast<GPR>(bit::extract<0, 4>(opcode));
    instr.rhsReg = static_cast<GPR>(bit::extract<8, 4>(opcode));
    instr.accReg = static_cast<GPR>(bit::extract<12, 4>(opcode));
    instr.y = bit::test<6>(opcode);
    instr.accumulate = !bit::test<5>(opcode); // Note the inverted bit!

    return instr;
}

// SMLAL<x><y>
inline auto SignedMultiplyAccumulateLong(uint32_t opcode) {
    arm::instrs::SignedMultiplyAccumulateLong instr{};

    instr.dstAccLoReg = static_cast<GPR>(bit::extract<12, 4>(opcode));
    instr.dstAccHiReg = static_cast<GPR>(bit::extract<16, 4>(opcode));
    instr.lhsReg = static_cast<GPR>(bit::extract<0, 4>(opcode));
    instr.rhsReg = static_cast<GPR>(bit::extract<8, 4>(opcode));
    instr.x = bit::test<5>(opcode);
    instr.y = bit::test<6>(opcode);

    return instr;
}

// MRS
inline auto PSRRead(uint32_t opcode) {
    arm::instrs::PSRRead instr{};

    instr.dstReg = static_cast<GPR>(bit::extract<12, 4>(opcode));
    instr.spsr = bit::test<22>(opcode);

    return instr;
}

// MSR
inline auto PSRWrite(uint32_t opcode) {
    arm::instrs::PSRWrite instr{};

    instr.immediate = bit::test<25>(opcode);
    instr.spsr = bit::test<22>(opcode);
    instr.f = bit::test<19>(opcode);
    instr.s = bit::test<18>(opcode);
    instr.x = bit::test<17>(opcode);
    instr.c = bit::test<16>(opcode);
    if (instr.immediate) {
        instr.value.imm = detail::DecodeRotatedImm(opcode).first;
    } else {
        instr.value.reg = static_cast<GPR>(bit::extract<0, 4>(opcode));
    }

    return instr;
}

// LDR,STR,LDRB,STRB
inline auto SingleDataTransfer(uint32_t opcode) {
    arm::instrs::SingleDataTransfer instr{};

    instr.preindexed = bit::test<24>(opcode);
    instr.byte = bit::test<22>(opcode);
    instr.writeback = bit::test<21>(opcode);
    instr.load = bit::test<20>(opcode);
    instr.reg = static_cast<GPR>(bit::extract<12, 4>(opcode));
    instr.address = detail::DecodeAddressing(opcode);

    return instr;
}

// LDRH,STRH,LDRSH,LDRSB,LDRD,STRD
inline auto HalfwordAndSignedTransfer(uint32_t opcode) {
    arm::instrs::HalfwordAndSignedTransfer instr{};

    instr.preindexed = bit::test<24>(opcode);
    instr.positiveOffset = bit::test<23>(opcode);
    instr.immediate = bit::test<22>(opcode);
    instr.writeback = bit::test<21>(opcode);
    instr.load = bit::test<20>(opcode);
    instr.sign = bit::test<6>(opcode);
    instr.half = bit::test<5>(opcode);
    instr.reg = static_cast<GPR>(bit::extract<12, 4>(opcode));
    instr.baseReg = static_cast<GPR>(bit::extract<16, 4>(opcode));
    if (instr.immediate) {
        instr.offset.imm = bit::extract<0, 4>(opcode) | (bit::extract<8, 4>(opcode) << 4);
    } else {
        instr.offset.reg = static_cast<GPR>(bit::extract<0, 4>(opcode));
    }

    return instr;
}

// LDM,STM
inline auto BlockTransfer(uint32_t opcode) {
    arm::instrs::BlockTransfer instr{};

    instr.preindexed = bit::test<24>(opcode);
    instr.positiveOffset = bit::test<23>(opcode);
    instr.userModeOrPSRTransfer = bit::test<22>(opcode);
    instr.writeback = bit::test<21>(opcode);
    instr.load = bit::test<20>(opcode);
    instr.baseReg = static_cast<GPR>(bit::extract<16, 4>(opcode));
    instr.regList = bit::extract<0, 16>(opcode);

    return instr;
}

// SWP,SWPB
inline auto SingleDataSwap(uint32_t opcode) {
    arm::instrs::SingleDataSwap instr{};

    instr.byte = bit::test<22>(opcode);
    instr.dstReg = static_cast<GPR>(bit::extract<12, 4>(opcode));
    instr.valueReg = static_cast<GPR>(bit::extract<0, 4>(opcode));
    instr.addressReg = static_cast<GPR>(bit::extract<16, 4>(opcode));

    return instr;
}

// SWI
inline auto SoftwareInterrupt(uint32_t opcode) {
    arm::instrs::SoftwareInterrupt instr{};

    instr.comment = bit::extract<0, 24>(opcode);

    return instr;
}

// BKPT
inline auto SoftwareBreakpoint(uint32_t opcode) {
    return arm::instrs::SoftwareBreakpoint{};
}

// PLD
inline auto Preload(uint32_t opcode) {
    arm::instrs::Preload instr{};

    instr.address = detail::DecodeAddressing(opcode);

    return instr;
}

// CDP,CDP2
inline auto CopDataOperations(uint32_t opcode, bool ext) {
    arm::instrs::CopDataOperations instr{};

    instr.opcode1 = bit::extract<20, 4>(opcode);
    instr.crn = bit::extract<16, 4>(opcode);
    instr.crd = bit::extract<12, 4>(opcode);
    instr.cpnum = bit::extract<8, 4>(opcode);
    instr.opcode2 = bit::extract<5, 3>(opcode);
    instr.crm = bit::extract<0, 4>(opcode);
    instr.ext = ext;

    return instr;
}

// STC,STC2,LDC,LDC2
inline auto CopDataTransfer(uint32_t opcode, bool ext) {
    arm::instrs::CopDataTransfer instr{};

    instr.preindexed = bit::test<24>(opcode);
    instr.positiveOffset = bit::test<23>(opcode);
    instr.n = bit::test<22>(opcode);
    instr.writeback = bit::test<21>(opcode);
    instr.load = bit::test<20>(opcode);
    instr.rn = static_cast<GPR>(bit::extract<16, 4>(opcode));
    instr.crd = bit::extract<12, 4>(opcode);
    instr.cpnum = bit::extract<8, 4>(opcode);
    instr.offset = bit::extract<0, 8>(opcode);
    instr.ext = ext;

    return instr;
}

// MCR,MCR2,MRC,MRC2
inline auto CopRegTransfer(uint32_t opcode, bool ext) {
    arm::instrs::CopRegTransfer instr{};

    instr.load = bit::test<20>(opcode);
    instr.reg.opcode1 = bit::extract<21, 3>(opcode);
    instr.reg.crn = bit::extract<16, 4>(opcode);
    instr.rd = static_cast<GPR>(bit::extract<12, 4>(opcode));
    instr.cpnum = bit::extract<8, 4>(opcode);
    instr.reg.opcode2 = bit::extract<5, 3>(opcode);
    instr.reg.crm = bit::extract<0, 4>(opcode);
    instr.ext = ext;

    return instr;
}

// MCRR,MRRC
inline auto CopDualRegTransfer(uint32_t opcode) {
    arm::instrs::CopDualRegTransfer instr{};

    instr.load = bit::test<20>(opcode);
    instr.rn = static_cast<GPR>(bit::extract<16, 4>(opcode));
    instr.rd = static_cast<GPR>(bit::extract<12, 4>(opcode));
    instr.cpnum = bit::extract<8, 4>(opcode);
    instr.opcode = bit::extract<4, 4>(opcode);
    instr.crm = bit::extract<0, 4>(opcode);

    return instr;
}

// UDF and other undefined instructions
inline auto Undefined() {
    return arm::instrs::Undefined{};
}

} // namespace armajitto::arm::arm_decoder
