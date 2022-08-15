#pragma once

#include "armajitto/util/bit_ops.hpp"
#include "decoder_client.hpp"

namespace armajitto::arm {

namespace detail {
    inline uint32_t DecodeRotatedImm(uint32_t opcode) {
        const uint8_t imm = bit::extract<0, 8>(opcode);
        const uint8_t rotate = bit::extract<8, 4>(opcode);
        return std::rotr(imm, rotate * 2);
    }

    inline auto DecodeShift(uint32_t opcode) {
        RegisterSpecifiedShift shift{};
        const uint8_t shiftParam = bit::extract<4, 8>(opcode);
        shift.type = static_cast<ShiftType>(bit::extract<1, 2>(shiftParam));
        shift.immediate = bit::test<0>(shiftParam);
        shift.srcReg = bit::extract<0, 4>(opcode);
        if (shift.immediate) {
            shift.amount.imm = bit::extract<3, 5>(shiftParam);
        } else {
            shift.amount.reg = bit::extract<4, 4>(shiftParam);
        }
        return shift;
    }

    inline auto DecodeAddressing(uint32_t opcode) {
        AddressingOffset offset{};
        offset.immediate = !bit::test<25>(opcode); // Note the inverted bit!
        offset.positiveOffset = bit::test<23>(opcode);
        offset.baseReg = bit::extract<16, 4>(opcode);
        if (offset.immediate) {
            offset.immValue = bit::extract<0, 12>(opcode);
        } else {
            offset.shift = DecodeShift(opcode);
        }
        return offset;
    }

    // B,BL
    inline auto Branch(uint32_t opcode, Condition cond, bool switchToThumb) {
        instrs::Branch instr{.cond = cond};

        instr.offset = bit::sign_extend<24>(bit::extract<24>(opcode)) << 2;
        instr.link = bit::test<24>(opcode);
        instr.switchToThumb = switchToThumb;

        return instr;
    }

    // BX,BLX
    inline auto BranchAndExchange(uint32_t opcode, Condition cond) {
        instrs::BranchAndExchange instr{.cond = cond};

        instr.reg = bit::extract<0, 4>(opcode);
        instr.link = bit::test<5>(opcode);

        return instr;
    }

    // AND,EOR,SUB,RSB,ADD,ADC,SBC,RSC,TST,TEQ,CMP,CMN,ORR,MOV,BIC,MVN
    inline auto DataProcessing(uint32_t opcode, Condition cond) {
        instrs::DataProcessing instr{.cond = cond};

        instr.opcode = static_cast<instrs::DataProcessing::Opcode>(bit::extract<21, 4>(opcode));
        instr.immediate = bit::test<25>(opcode);
        instr.setFlags = bit::test<20>(opcode);
        instr.dstReg = bit::extract<12, 4>(opcode);
        instr.lhsReg = bit::extract<16, 4>(opcode);
        if (instr.immediate) {
            instr.rhs.imm = DecodeRotatedImm(opcode);
        } else {
            instr.rhs.shift = DecodeShift(opcode);
        }

        return instr;
    }

    // CLZ
    inline auto CountLeadingZeros(uint32_t opcode, Condition cond) {
        instrs::CountLeadingZeros instr{.cond = cond};

        instr.dstReg = bit::extract<12, 4>(opcode);
        instr.argReg = bit::extract<0, 4>(opcode);

        return instr;
    }

    // QADD,QSUB,QDADD,QDSUB
    inline auto SaturatingAddSub(uint32_t opcode, Condition cond) {
        instrs::SaturatingAddSub instr{.cond = cond};

        instr.dstReg = bit::extract<12, 4>(opcode);
        instr.lhsReg = bit::extract<0, 4>(opcode);
        instr.rhsReg = bit::extract<16, 4>(opcode);
        instr.sub = bit::test<21>(opcode);
        instr.dbl = bit::test<22>(opcode);

        return instr;
    }

    // MUL,MLA
    inline auto MultiplyAccumulate(uint32_t opcode, Condition cond) {
        instrs::MultiplyAccumulate instr{.cond = cond};

        instr.dstReg = bit::extract<16, 4>(opcode);
        instr.lhsReg = bit::extract<0, 4>(opcode);
        instr.rhsReg = bit::extract<8, 4>(opcode);
        instr.accReg = bit::extract<12, 4>(opcode);
        instr.accumulate = bit::test<21>(opcode);
        instr.setFlags = bit::test<20>(opcode);

        return instr;
    }

    // SMULL,UMULL,SMLAL,UMLAL
    inline auto MultiplyAccumulateLong(uint32_t opcode, Condition cond) {
        instrs::MultiplyAccumulateLong instr{.cond = cond};

        instr.dstAccLoReg = bit::extract<12, 4>(opcode);
        instr.dstAccHiReg = bit::extract<16, 4>(opcode);
        instr.lhsReg = bit::extract<0, 4>(opcode);
        instr.rhsReg = bit::extract<8, 4>(opcode);
        instr.signedMul = bit::test<22>(opcode);
        instr.accumulate = bit::test<21>(opcode);
        instr.setFlags = bit::test<20>(opcode);

        return instr;
    }

    // SMUL<x><y>,SMLA<x><y>
    inline auto SignedMultiplyAccumulate(uint32_t opcode, Condition cond) {
        instrs::SignedMultiplyAccumulate instr{.cond = cond};

        instr.dstReg = bit::extract<16, 4>(opcode);
        instr.lhsReg = bit::extract<0, 4>(opcode);
        instr.rhsReg = bit::extract<8, 4>(opcode);
        instr.accReg = bit::extract<12, 4>(opcode);
        instr.x = bit::test<5>(opcode);
        instr.y = bit::test<6>(opcode);
        instr.accumulate = !bit::test<21>(opcode); // Note the inverted bit!

        return instr;
    }

    // SMULW<y>,SMLAW<y>
    inline auto SignedMultiplyAccumulateWord(uint32_t opcode, Condition cond) {
        instrs::SignedMultiplyAccumulateWord instr{.cond = cond};

        instr.dstReg = bit::extract<16, 4>(opcode);
        instr.lhsReg = bit::extract<0, 4>(opcode);
        instr.rhsReg = bit::extract<8, 4>(opcode);
        instr.accReg = bit::extract<12, 4>(opcode);
        instr.y = bit::test<6>(opcode);
        instr.accumulate = !bit::test<5>(opcode); // Note the inverted bit!

        return instr;
    }

    // SMLAL<x><y>
    inline auto SignedMultiplyAccumulateLong(uint32_t opcode, Condition cond) {
        instrs::SignedMultiplyAccumulateLong instr{.cond = cond};

        instr.dstAccLoReg = bit::extract<12, 4>(opcode);
        instr.dstAccHiReg = bit::extract<16, 4>(opcode);
        instr.lhsReg = bit::extract<0, 4>(opcode);
        instr.rhsReg = bit::extract<8, 4>(opcode);
        instr.x = bit::test<5>(opcode);
        instr.y = bit::test<6>(opcode);

        return instr;
    }

    // MRS
    inline auto PSRRead(uint32_t opcode, Condition cond) {
        instrs::PSRRead instr{.cond = cond};

        instr.dstReg = bit::extract<12, 4>(opcode);
        instr.spsr = bit::test<22>(opcode);

        return instr;
    }

    // MSR
    inline auto PSRWrite(uint32_t opcode, Condition cond) {
        instrs::PSRWrite instr{.cond = cond};

        instr.immediate = bit::test<25>(opcode);
        instr.spsr = bit::test<22>(opcode);
        instr.f = bit::test<19>(opcode);
        instr.s = bit::test<18>(opcode);
        instr.x = bit::test<17>(opcode);
        instr.c = bit::test<16>(opcode);
        if (instr.immediate) {
            instr.value.imm = DecodeRotatedImm(opcode);
        } else {
            instr.value.reg = bit::extract<0, 4>(opcode);
        }

        return instr;
    }

    // LDR,STR,LDRB,STRB
    inline auto SingleDataTransfer(uint32_t opcode, Condition cond) {
        instrs::SingleDataTransfer instr{.cond = cond};

        instr.preindexed = bit::test<24>(opcode);
        instr.byte = bit::test<22>(opcode);
        instr.writeback = bit::test<21>(opcode);
        instr.load = bit::test<20>(opcode);
        instr.dstReg = bit::extract<12, 4>(opcode);
        instr.offset = DecodeAddressing(opcode);

        return instr;
    }

    // LDRH,STRH,LDRSH,LDRSB,LDRD,STRD
    inline auto HalfwordAndSignedTransfer(uint32_t opcode, Condition cond) {
        instrs::HalfwordAndSignedTransfer instr{.cond = cond};

        instr.preindexed = bit::test<24>(opcode);
        instr.positiveOffset = bit::test<23>(opcode);
        instr.immediate = bit::test<22>(opcode);
        instr.writeback = bit::test<21>(opcode);
        instr.load = bit::test<20>(opcode);
        instr.sign = bit::test<6>(opcode);
        instr.half = bit::test<5>(opcode);
        instr.dstReg = bit::extract<12, 4>(opcode);
        instr.baseReg = bit::extract<16, 4>(opcode);
        if (instr.immediate) {
            instr.offset.imm = bit::extract<0, 8>(opcode);
        } else {
            instr.offset.reg = bit::extract<0, 4>(opcode);
        }

        return instr;
    }

    // LDM,STM
    inline auto BlockTransfer(uint32_t opcode, Condition cond) {
        instrs::BlockTransfer instr{.cond = cond};

        instr.preindexed = bit::test<24>(opcode);
        instr.positiveOffset = bit::test<23>(opcode);
        instr.userMode = bit::test<22>(opcode);
        instr.writeback = bit::test<21>(opcode);
        instr.load = bit::test<20>(opcode);
        instr.baseReg = bit::extract<16, 4>(opcode);
        instr.regList = bit::extract<0, 16>(opcode);

        return instr;
    }

    // SWP,SWPB
    inline auto SingleDataSwap(uint32_t opcode, Condition cond) {
        instrs::SingleDataSwap instr{.cond = cond};

        instr.byte = bit::test<22>(opcode);
        instr.dstReg = bit::extract<12, 4>(opcode);
        instr.valueReg = bit::extract<0, 4>(opcode);
        instr.addressReg = bit::extract<16, 4>(opcode);

        return instr;
    }

    // SWI
    inline auto SoftwareInterrupt(uint32_t opcode, Condition cond) {
        return instrs::SoftwareInterrupt{.cond = cond, .comment = bit::extract<0, 24>(opcode)};
    }

    // BKPT
    inline auto SoftwareBreakpoint(uint32_t opcode, Condition cond) {
        return instrs::SoftwareBreakpoint{.cond = cond};
    }

    // PLD
    inline auto Preload(uint32_t opcode) {
        instrs::Preload instr{};

        instr.offset = DecodeAddressing(opcode);

        return instr;
    }

    // CDP,CDP2
    inline auto CopDataOperations(uint32_t opcode, Condition cond, bool ext) {
        instrs::CopDataOperations instr{.cond = cond};

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
    inline auto CopDataTransfer(uint32_t opcode, Condition cond, bool ext) {
        instrs::CopDataTransfer instr{.cond = cond};

        instr.preindexed = bit::test<24>(opcode);
        instr.positiveOffset = bit::test<23>(opcode);
        instr.n = bit::test<22>(opcode);
        instr.writeback = bit::test<21>(opcode);
        instr.load = bit::test<20>(opcode);
        instr.rn = bit::extract<16, 4>(opcode);
        instr.crd = bit::extract<12, 4>(opcode);
        instr.cpnum = bit::extract<8, 4>(opcode);
        instr.offset = bit::extract<0, 8>(opcode);
        instr.ext = ext;

        return instr;
    }

    // MCR,MCR2,MRC,MRC2
    inline auto CopRegTransfer(uint32_t opcode, Condition cond, bool ext) {
        instrs::CopRegTransfer instr{.cond = cond};

        instr.store = bit::test<20>(opcode);
        instr.opcode1 = bit::extract<21, 3>(opcode);
        instr.crn = bit::extract<16, 4>(opcode);
        instr.rd = bit::extract<12, 4>(opcode);
        instr.cpnum = bit::extract<8, 4>(opcode);
        instr.opcode2 = bit::extract<5, 3>(opcode);
        instr.crm = bit::extract<0, 4>(opcode);
        instr.ext = ext;

        return instr;
    }

    // MCRR,MRRC
    inline auto CopDualRegTransfer(uint32_t opcode, Condition cond) {
        instrs::CopDualRegTransfer instr{.cond = cond};

        instr.store = bit::test<20>(opcode);
        instr.rn = bit::extract<16, 4>(opcode);
        instr.rd = bit::extract<12, 4>(opcode);
        instr.cpnum = bit::extract<8, 4>(opcode);
        instr.opcode = bit::extract<4, 4>(opcode);
        instr.crm = bit::extract<0, 4>(opcode);

        return instr;
    }

    // UDF and other undefined instructions
    inline auto Undefined(Condition cond) {
        return instrs::Undefined{.cond = cond};
    }
} // namespace detail

template <DecoderClient TClient>
inline DecoderAction DecodeARM(TClient &client, uint32_t address) {
    using namespace detail;

    const CPUArch arch = client.GetCPUArch();
    const uint32_t opcode = client.CodeReadWord(address);

    const auto cond = static_cast<Condition>(bit::extract<28, 4>(opcode));

    const uint32_t op = bit::extract<25, 3>(opcode);
    const uint32_t bits24to20 = bit::extract<20, 5>(opcode);
    const uint32_t bits7to4 = bit::extract<4, 4>(opcode);

    if (arch == CPUArch::ARMv5TE) {
        if (cond == Condition::NV) {
            switch (op) {
            case 0b000: return client.Process(Undefined(Condition::AL));
            case 0b001: return client.Process(Undefined(Condition::AL));
            case 0b100: return client.Process(Undefined(Condition::AL));
            case 0b010:
            case 0b011:
                if ((bits24to20 & 0b1'0111) == 0b1'0101) {
                    return client.Process(Preload(opcode, Condition::AL));
                } else {
                    return client.Process(Undefined(cond));
                }
            case 0b110: return client.Process(CopDataTransfer(opcode, Condition::AL, true));
            case 0b111:
                if (!bit::test<24>(opcode)) {
                    if (bit::test<4>(opcode)) {
                        return client.Process(CopRegTransfer(opcode, Condition::AL, true));
                    } else {
                        return client.Process(CopDataOperations(opcode, Condition::AL, true));
                    }
                }
                if (bit::test<8>(opcode)) {
                    return client.Process(Undefined(cond));
                }
                break;
            }
        }
    }

    switch (op) {
    case 0b000:
        if ((bits24to20 & 0b1'1111) == 0b1'0010 && (bits7to4 & 0b1111) == 0b0001) {
            return client.Process(BranchAndExchange(opcode, cond));
        } else if ((bits24to20 & 0b1'1111) == 0b1'0010 && (bits7to4 & 0b1111) == 0b0011) {
            if (arch == CPUArch::ARMv5TE) {
                return client.Process(BranchAndExchange(opcode, cond));
            } else {
                return client.Process(Undefined(cond));
            }
        } else if ((bits24to20 & 0b1'1111) == 0b1'0110 && (bits7to4 & 0b1111) == 0b0001) {
            if (arch == CPUArch::ARMv5TE) {
                return client.Process(CountLeadingZeros(opcode, cond));
            } else {
                return client.Process(Undefined(cond));
            }
        } else if ((bits24to20 & 0b1'1111) == 0b1'0010 && (bits7to4 & 0b1111) == 0b0111) {
            if (arch == CPUArch::ARMv5TE) {
                return client.Process(SoftwareBreakpoint(opcode, cond));
            } else {
                return client.Process(Undefined(cond));
            }
        } else if ((bits24to20 & 0b1'1001) == 0b1'0000 && (bits7to4 & 0b1111) == 0b0101) {
            if (arch == CPUArch::ARMv5TE) {
                return client.Process(SaturatingAddSub(opcode, cond));
            } else {
                return client.Process(Undefined(cond));
            }
        } else if ((bits24to20 & 0b1'1001) == 0b1'0000 && (bits7to4 & 0b1001) == 0b1000) {
            if (arch == CPUArch::ARMv5TE) {
                const uint8_t op = bit::extract<21, 2>(opcode);
                switch (op) {
                case 0b00:
                case 0b11: return client.Process(SignedMultiplyAccumulate(opcode, cond));
                case 0b01: return client.Process(SignedMultiplyAccumulateWord(opcode, cond));
                case 0b10: return client.Process(SignedMultiplyAccumulateLong(opcode, cond));
                }
            } else {
                return Undefined(opcode, cond);
            }
        } else if ((bits24to20 & 0b1'1100) == 0b0'0000 && (bits7to4 & 0b1111) == 0b1001) {
            return client.Process(MultiplyAccumulate(opcode, cond));
        } else if ((bits24to20 & 0b1'1000) == 0b0'1000 && (bits7to4 & 0b1111) == 0b1001) {
            return client.Process(MultiplyAccumulateLong(opcode, cond));
        } else if ((bits24to20 & 0b1'1011) == 0b1'0000 && (bits7to4 & 0b1111) == 0b1001) {
            return client.Process(SingleDataSwap(opcode, cond));
        } else if ((bits7to4 & 0b1001) == 0b1001) {
            const bool bit12 = bit::test<12>(opcode);
            const bool l = bit::test<20>(opcode);
            const bool s = bit::test<6>(opcode);
            const bool h = bit::test<5>(opcode);
            if (l) {
                return client.Process(HalfwordAndSignedTransfer(opcode, cond));
            } else {
                if (s && h) {
                    if (arch == CPUArch::ARMv5TE) {
                        if (bit12) {
                            return client.Process(Undefined(opcode, cond));
                        } else {
                            return client.Process(HalfwordAndSignedTransfer(opcode, cond));
                        }
                    }
                } else if (s) {
                    if (arch == CPUArch::ARMv5TE) {
                        if (bit12) {
                            return client.Process(Undefined(opcode, cond));
                        } else {
                            return client.Process(HalfwordAndSignedTransfer(opcode, cond));
                        }
                    }
                } else if (h) {
                    return client.Process(HalfwordAndSignedTransfer(opcode, cond));
                }
            }
        } else if ((bits24to20 & 0b1'1011) == 0b1'0000 && (bits7to4 & 0b1111) == 0b0000) {
            return client.Process(PSRRead(opcode, cond));
        } else if ((bits24to20 & 0b1'1011) == 0b1'0010 && (bits7to4 & 0b1111) == 0b0000) {
            return client.Process(PSRWrite(opcode, cond));
        } else {
            return client.Process(DataProcessing(opcode, cond));
        }
        break;
    case 0b001:
        if ((bits24to20 & 0b1'1011) == 0b1'0010) {
            return client.Process(PSRWrite(opcode, cond));
        } else if ((bits24to20 & 0b1'1011) == 0b1'0000) {
            return client.Process(Undefined(opcode, cond));
        } else {
            return client.Process(DataProcessing(opcode, cond));
        }
        break;
    case 0b010:
    case 0b011:
        if (bit::test<0>(op) && bit::test<0>(bits7to4)) {
            return client.Process(Undefined(opcode, cond));
        } else {
            return client.Process(SingleDataTransfer(opcode, cond));
        }
        break;
    case 0b100: return client.Process(BlockTransfer(opcode, cond));
    case 0b101: {
        const bool switchToThumb = (arch == CPUArch::ARMv5TE) && (cond == Condition::NV);
        return client.Process(Branch(opcode, cond, switchToThumb));
    }
    case 0b110:
        if (arch == CPUArch::ARMv5TE) {
            if ((bits24to20 & 0b1'1110) == 0b0'0100) {
                return client.Process(CopDualRegTransfer(opcode, cond));
            }
        }
        return client.Process(CopDataTransfer(opcode, cond, false));
    case 0b111: {
        if (bit::test<24>(opcode)) {
            return client.Process(SoftwareInterrupt(opcode, cond));
        } else {
            if (bit::test<4>(opcode)) {
                return client.Process(CopRegTransfer(opcode, cond, false));
            } else {
                return client.Process(CopDataOperations(opcode, cond, false));
            }
        }
        break;
    }
    }

    return Action::UnmappedInstruction;
}

} // namespace armajitto::arm
