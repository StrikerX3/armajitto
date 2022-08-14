#pragma once

#include "decoder_client.hpp"
#include "util/bit_ops.hpp"

namespace armajitto::arm::decoder {

namespace detail {
    auto SimpleRegShift(uint8_t reg) {
        RegisterSpecifiedShift shift{};
        shift.type = ShiftType::LSL;
        shift.immediate = true;
        shift.srcReg = reg;
        shift.amount.imm = 0;
        return shift;
    }

    template <Client TClient>
    Action ShiftByImm(TClient &client, uint16_t opcode) {
        instrs::DataProcessing instr{.cond = Condition::AL};

        instr.opcode = instrs::DataProcessing::Opcode::MOV;
        instr.immediate = false;
        instr.setFlags = true;
        instr.dstReg = bit::extract<0, 3>(opcode);
        instr.lhsReg = instr.dstReg;

        const uint8_t op = bit::extract<11, 2>(opcode);
        switch (op) {
        case 0b00: instr.rhs.shift.type = ShiftType::LSL; break;
        case 0b01: instr.rhs.shift.type = ShiftType::LSR; break;
        case 0b10: instr.rhs.shift.type = ShiftType::ASR; break;
        default: break; // TODO: unreachable
        }
        instr.rhs.shift.immediate = true;
        instr.rhs.shift.srcReg = bit::extract<3, 3>(opcode);
        instr.rhs.shift.amount.imm = bit::extract<6, 5>(opcode);

        return client.Process(instr);
    }

    template <Client TClient>
    Action AddSubRegImm(TClient &client, uint16_t opcode) {
        instrs::DataProcessing instr{.cond = Condition::AL};

        if (bit::test<9>(opcode)) {
            instr.opcode = instrs::DataProcessing::Opcode::SUB;
        } else {
            instr.opcode = instrs::DataProcessing::Opcode::ADD;
        }
        instr.immediate = bit::test<10>(opcode);
        instr.setFlags = true;
        instr.dstReg = bit::extract<0, 3>(opcode);
        instr.lhsReg = bit::extract<3, 3>(opcode);
        if (instr.immediate) {
            instr.rhs.imm = bit::extract<6, 3>(opcode);
        } else {
            instr.rhs.shift = SimpleRegShift(bit::extract<6, 3>(opcode));
        }

        return client.Process(instr);
    }

    template <Client TClient>
    Action MovCmpAddSubImm(TClient &client, uint16_t opcode) {
        instrs::DataProcessing instr{.cond = Condition::AL};

        switch (bit::extract<11, 2>(opcode)) {
        case 0b00: instr.opcode = instrs::DataProcessing::Opcode::MOV;
        case 0b01: instr.opcode = instrs::DataProcessing::Opcode::CMP;
        case 0b10: instr.opcode = instrs::DataProcessing::Opcode::ADD;
        case 0b11: instr.opcode = instrs::DataProcessing::Opcode::SUB;
        }
        instr.immediate = true;
        instr.setFlags = true;
        instr.dstReg = bit::extract<8, 3>(opcode);
        instr.lhsReg = instr.dstReg;
        instr.rhs.imm = bit::extract<0, 8>(opcode);

        return client.Process(instr);
    }

    template <Client TClient>
    Action DataProcessing(TClient &client, uint16_t opcode) {
        using DPOpcode = instrs::DataProcessing::Opcode;

        auto processDP = [&](DPOpcode dpOpcode) {
            instrs::DataProcessing instr{.cond = Condition::AL};

            instr.opcode = dpOpcode;
            instr.immediate = false;
            instr.setFlags = true;
            instr.dstReg = bit::extract<12, 3>(opcode);
            instr.lhsReg = instr.dstReg;
            instr.rhs.shift = SimpleRegShift(bit::extract<0, 3>(opcode));

            return client.Process(instr);
        };

        auto processShift = [&](ShiftType shiftType) {
            instrs::DataProcessing instr{.cond = Condition::AL};

            instr.opcode = instrs::DataProcessing::Opcode::MOV;
            instr.immediate = false;
            instr.setFlags = true;
            instr.dstReg = bit::extract<0, 3>(opcode);
            instr.lhsReg = 0;
            instr.rhs.shift.type = shiftType;
            instr.rhs.shift.immediate = false;
            instr.rhs.shift.srcReg = instr.dstReg;
            instr.rhs.shift.amount.reg = bit::extract<3, 3>(opcode);

            return client.Process(instr);
        };

        auto processNeg = [&]() {
            instrs::DataProcessing instr{.cond = Condition::AL};

            instr.opcode = DPOpcode::RSB;
            instr.immediate = true;
            instr.setFlags = true;
            instr.dstReg = bit::extract<12, 3>(opcode);
            instr.lhsReg = instr.dstReg;
            instr.rhs.imm = 0;

            return client.Process(instr);
        };

        auto processMul = [&]() {
            instrs::MultiplyAccumulate instr{.cond = Condition::AL};

            instr.dstReg = bit::extract<0, 3>(opcode);
            instr.lhsReg = instr.dstReg;
            instr.rhsReg = bit::extract<3, 3>(opcode);
            instr.accReg = 0;
            instr.accumulate = false;
            instr.setFlags = true;

            return client.Process(instr);
        };

        enum class Op : uint8_t { AND, EOR, LSL, LSR, ASR, ADC, SBC, ROR, TST, NEG, CMP, CMN, ORR, MUL, BIC, MVN };
        const auto op = static_cast<Op>(bit::extract<6, 4>(opcode));
        switch (op) {
        case Op::AND: return processDP(DPOpcode::AND);
        case Op::EOR: return processDP(DPOpcode::EOR);
        case Op::LSL: return processShift(ShiftType::LSL);
        case Op::LSR: return processShift(ShiftType::LSR);
        case Op::ASR: return processShift(ShiftType::ASR);
        case Op::ADC: return processDP(DPOpcode::ADC);
        case Op::SBC: return processDP(DPOpcode::SBC);
        case Op::ROR: return processShift(ShiftType::ROR);
        case Op::TST: return processDP(DPOpcode::TST);
        case Op::NEG: return processNeg();
        case Op::CMP: return processDP(DPOpcode::CMP);
        case Op::CMN: return processDP(DPOpcode::CMN);
        case Op::ORR: return processDP(DPOpcode::ORR);
        case Op::MUL: return processMul();
        case Op::BIC: return processDP(DPOpcode::BIC);
        case Op::MVN: return processDP(DPOpcode::MVN);
        }
    }

} // namespace detail

template <Client TClient>
Action DecodeThumb(TClient &client, uint32_t address) {
    using namespace detail;

    const CPUArch arch = client.GetCPUArch();
    const uint16_t opcode = client.CodeReadHalf(address);

    const uint8_t group = bit::extract<12, 4>(opcode);
    switch (group) {
    case 0b0000:
    case 0b0001:
        if (bit::extract<11, 2>(opcode) == 0b11) {
            return AddSubRegImm(client, opcode);
        } else {
            return ShiftByImm(client, opcode);
        }
    case 0b0010:
    case 0b0011: return MovCmpAddSubImm(client, opcode);
    case 0b0100:
        if (((opcode >> 10) & 0b11) == 0b00) {
            return DataProcessing(client, opcode);
        } else if (((opcode >> 10) & 0b11) == 0b01) {
            const uint8_t op = (opcode >> 8) & 0b11;
            const bool h1 = (opcode >> 7) & 1;
            switch (op) {
            case 0b00: return ADDHiReg(opcode);
            case 0b01: return CMPHiReg(opcode);
            case 0b10: return MOVHiReg(opcode);
            case 0b11:
                if (arch == CPUArch::ARMv5TE) {
                    if (h1) {
                        return BLXHiReg(opcode);
                    }
                }
                return BXHiReg(opcode);
            }
        } else {
            return LDRPCRel(opcode);
        }
    case 0b0101:
        if ((opcode >> 9) & 1) {
            const bool h = (opcode >> 11) & 1;
            const bool s = (opcode >> 10) & 1;
            return h ? (s ? LDRSHRegOffset(opcode) : LDRHRegOffset(opcode))
                     : (s ? LDRSBRegOffset(opcode) : STRHRegOffset(opcode));
        } else {
            const bool l = (opcode >> 11) & 1;
            const bool b = (opcode >> 10) & 1;
            return l ? (b ? LDRBRegOffset(opcode) : LDRRegOffset(opcode))
                     : (b ? STRBRegOffset(opcode) : STRRegOffset(opcode));
        }
    case 0b0110:
    case 0b0111: {
        const bool b = (opcode >> 12) & 1;
        const bool l = (opcode >> 11) & 1;
        return b ? (l ? LDRBImmOffset(opcode) : STRBImmOffset(opcode))
                 : (l ? LDRImmOffset(opcode) : STRImmOffset(opcode));
    }
    case 0b1000: {
        const bool l = (opcode >> 11) & 1;
        return l ? LDRHImmOffset(opcode) : STRHImmOffset(opcode);
    }
    case 0b1001: {
        const bool l = (opcode >> 11) & 1;
        return l ? LDRSPRel(opcode) : STRSPRel(opcode);
    }
    case 0b1010: {
        const bool sp = (opcode >> 11) & 1;
        return sp ? ADDSP(opcode) : ADDPC(opcode);
    }
    case 0b1011:
        if (((opcode >> 8) & 0b1111) == 0b0000) {
            return ADDSUBSP(opcode);
        } else if (((opcode >> 8) & 0b1111) == 0b1110) {
            if (arch == CPUArch::ARMv5TE) {
                return BKPT(opcode);
            } else {
                return UDF(opcode);
            }
        } else if (((opcode >> 8) & 0b0110) == 0b0100) {
            const bool l = (opcode >> 11) & 1;
            return l ? POP(opcode) : PUSH(opcode);
        } else {
            return UDF(opcode);
        }
    case 0b1100: {
        const bool l = (opcode >> 11) & 1;
        return l ? LDM(opcode) : STM(opcode);
    }
    case 0b1101:
        if (((opcode >> 8) & 0b1111) == 0b1111) {
            return SWI(opcode);
        } else if (((opcode >> 8) & 0b1111) == 0b1110) {
            return UDF(opcode);
        } else {
            return BCond(opcode);
        }
    case 0b1110: {
        if (arch == CPUArch::ARMv5TE) {
            const bool blx = (opcode >> 11) & 1;
            if (blx) {
                if (opcode & 1) {
                    return UDF(opcode);
                } else {
                    // BLX suffix
                    const uint32_t suffixOffset = (opcode & 0x7FF);
                    const uint16_t prefix = readFn(context, address - 2);
                    if ((prefix & 0xF800) == 0xF000) {
                        // Valid prefix
                        const uint32_t prefixOffset = (prefix & 0x7FF);
                        const int32_t offset = bit::sign_extend<23>(prefixOffset << 12) | (suffixOffset << 1);
                        return CompleteLongBranchSuffix(opcode, offset, true, prefix);
                    } else {
                        // Invalid prefix
                        return PartialLongBranchSuffix(opcode, suffixOffset << 1, true);
                    }
                }
            }
        }
        return B(opcode);
    }
    case 0b1111: {
        const bool h = (opcode >> 11) & 1;
        if (h) {
            // BL suffix
            const uint32_t suffixOffset = (opcode & 0x7FF);
            const uint16_t prefix = readFn(context, address - 2);
            if ((prefix & 0xF800) == 0xF000) {
                // Valid prefix
                const uint32_t prefixOffset = (prefix & 0x7FF);
                const int32_t offset = bit::sign_extend<23>(prefixOffset << 12) | (suffixOffset << 1);
                return CompleteLongBranchSuffix(opcode, offset, false, prefix);
            } else {
                // Invalid prefix
                return PartialLongBranchSuffix(opcode, suffixOffset << 1, false);
            }
        } else {
            // BL/BLX prefix
            const uint32_t prefixOffset = (opcode & 0x7FF);
            const uint16_t suffix = readFn(context, address + 2);
            if ((suffix & 0xF800) == 0xF800) {
                // Valid BL suffix
                const uint32_t suffixOffset = (suffix & 0x7FF);
                const int32_t offset = bit::sign_extend<23>(prefixOffset << 12) | (suffixOffset << 1);
                return CompleteLongBranchPrefix(opcode, offset, false, suffix);
            } else if (arch == CPUArch::ARMv5TE && (suffix & 0xF801) == 0xE800) {
                // Valid BLX suffix
                const uint32_t suffixOffset = (suffix & 0x7FF);
                const int32_t offset = bit::sign_extend<23>(prefixOffset << 12) | (suffixOffset << 1);
                return CompleteLongBranchPrefix(opcode, offset, true, suffix);
            } else {
                // Invalid suffix
                return PartialLongBranchPrefix(opcode, bit::sign_extend<23>(prefixOffset << 12));
            }
        }
    }
    }

    return Action::UnmappedInstruction
}

} // namespace armajitto::arm::decoder
