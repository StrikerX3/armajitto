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

    template <Client TClient>
    Action HiRegOps(TClient &client, uint16_t opcode) {
        instrs::DataProcessing instr{.cond = Condition::AL};

        const uint8_t h1 = bit::extract<7>(opcode);
        const uint8_t h2 = bit::extract<6>(opcode);
        switch (bit::extract<8, 2>(opcode)) {
        case 0b00: instr.opcode = instrs::DataProcessing::Opcode::ADD; break;
        case 0b01: instr.opcode = instrs::DataProcessing::Opcode::CMP; break;
        case 0b10: instr.opcode = instrs::DataProcessing::Opcode::MOV; break;
        }
        instr.immediate = false;
        instr.setFlags = false;
        instr.dstReg = bit::extract<0, 3>(opcode) + h1 * 8;
        instr.lhsReg = 0;
        instr.rhs.shift = SimpleRegShift(bit::extract<3, 3>(opcode) + h2 * 8);

        return client.Process(instr);
    }

    template <Client TClient>
    Action HiRegBranchExchange(TClient &client, uint16_t opcode, bool link) {
        instrs::BranchAndExchange instr{.cond = Condition::AL};

        const uint8_t h2 = bit::extract<6>(opcode);
        instr.reg = bit::extract<3, 3>(opcode) + h2 * 8;
        instr.link = link;

        return client.Process(instr);
    }

    template <Client TClient>
    Action PCRelativeLoad(TClient &client, uint16_t opcode) {
        instrs::SingleDataTransfer instr{.cond = Condition::AL};

        instr.preindexed = true;
        instr.byte = false;
        instr.writeback = false;
        instr.load = true;
        instr.dstReg = bit::extract<8, 3>(opcode);
        instr.offset.immediate = true;
        instr.offset.positiveOffset = true;
        instr.offset.baseReg = 15;
        instr.offset.immValue = bit::extract<0, 8>(opcode) * 4;

        return client.Process(instr);
    }

    template <Client TClient>
    Action LoadStoreRegOffset(TClient &client, uint16_t opcode) {
        if (bit::test<9>(opcode)) {
            instrs::HalfwordAndSignedTransfer instr{.cond = Condition::AL};

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

            instr.dstReg = bit::extract<0, 3>(opcode);
            instr.baseReg = bit::extract<3, 3>(opcode);
            instr.offset.reg = bit::extract<6, 3>(opcode);

            return client.Process(instr);
        } else {
            instrs::SingleDataTransfer instr{.cond = Condition::AL};

            instr.preindexed = true;
            instr.byte = bit::test<10>(opcode);
            instr.writeback = false;
            instr.load = bit::test<11>(opcode);
            instr.dstReg = bit::extract<0, 3>(opcode);
            instr.offset.immediate = false;
            instr.offset.positiveOffset = true;
            instr.offset.baseReg = bit::extract<3, 3>(opcode);
            instr.offset.shift = SimpleRegShift(bit::extract<6, 3>(opcode));

            return client.Process(instr);
        }
    }

    template <Client TClient>
    Action LoadStoreByteWordImmOffset(TClient &client, uint16_t opcode) {
        instrs::SingleDataTransfer instr{.cond = Condition::AL};

        instr.preindexed = true;
        instr.byte = bit::test<12>(opcode);
        instr.writeback = false;
        instr.load = bit::test<11>(opcode);
        instr.dstReg = bit::extract<0, 3>(opcode);
        instr.offset.immediate = true;
        instr.offset.positiveOffset = true;
        instr.offset.baseReg = bit::extract<3, 3>(opcode);
        instr.offset.immValue = bit::extract<6, 5>(opcode);

        return client.Process(instr);
    }

    template <Client TClient>
    Action LoadStoreHalfImmOffset(TClient &client, uint16_t opcode) {
        instrs::HalfwordAndSignedTransfer instr{.cond = Condition::AL};

        instr.preindexed = true;
        instr.positiveOffset = true;
        instr.immediate = true;
        instr.writeback = false;
        instr.load = bit::test<11>(opcode);
        instr.sign = false;
        instr.half = true;
        instr.dstReg = bit::extract<0, 3>(opcode);
        instr.baseReg = bit::extract<3, 3>(opcode);
        instr.offset.imm = bit::extract<6, 5>(opcode);

        return client.Process(instr);
    }

    template <Client TClient>
    Action SPRelativeLoadStore(TClient &client, uint16_t opcode) {
        instrs::SingleDataTransfer instr{.cond = Condition::AL};

        instr.preindexed = true;
        instr.byte = false;
        instr.writeback = false;
        instr.load = bit::test<11>(opcode);
        instr.dstReg = bit::extract<8, 3>(opcode);
        instr.offset.immediate = true;
        instr.offset.positiveOffset = true;
        instr.offset.baseReg = 13;
        instr.offset.immValue = bit::extract<0, 8>(opcode) * 4;

        return client.Process(instr);
    }

    template <Client TClient>
    Action AddToSPOrPC(TClient &client, uint16_t opcode) {
        instrs::DataProcessing instr{.cond = Condition::AL};

        instr.opcode = instrs::DataProcessing::Opcode::ADD;
        instr.immediate = true;
        instr.setFlags = false;
        instr.dstReg = bit::extract<8, 3>(opcode);
        instr.lhsReg = bit::test<11>(opcode) ? 13 : 15;
        instr.rhs.imm = bit::extract<0, 8>(opcode) * 4;

        return client.Process(instr);
    }

    template <Client TClient>
    Action AdjustSP(TClient &client, uint16_t opcode) {
        instrs::DataProcessing instr{.cond = Condition::AL};

        if (bit::test<7>(opcode)) {
            instr.opcode = instrs::DataProcessing::Opcode::SUB;
        } else {
            instr.opcode = instrs::DataProcessing::Opcode::ADD;
        }
        instr.immediate = true;
        instr.setFlags = false;
        instr.dstReg = 13;
        instr.lhsReg = 13;
        instr.rhs.imm = bit::extract<0, 7>(opcode) * 4;

        return client.Process(instr);
    }

    template <Client TClient>
    Action PushPop(TClient &client, uint16_t opcode) {
        instrs::BlockTransfer instr{.cond = Condition::AL};

        //                   P U S W L   reg included by R bit
        // PUSH = STMDB sp!  + - - + -   LR
        // POP  = LDMIA sp!  - + - + +   PC
        const bool load = bit::test<11>(opcode);
        instr.preindexed = !load;
        instr.positiveOffset = load;
        instr.userMode = false;
        instr.writeback = true;
        instr.load = load;
        instr.baseReg = 13;
        instr.regList = bit::extract<0, 8>(opcode);
        if (bit::test<8>(opcode)) {
            if (load) {
                instr.regList |= (1 << 15);
            } else {
                instr.regList |= (1 << 14);
            }
        }

        return client.Process(instr);
    }

    template <Client TClient>
    Action LoadStoreMultiple(TClient &client, uint16_t opcode) {
        instrs::BlockTransfer instr{.cond = Condition::AL};

        // load  P U S W L
        //   -   - + - + -
        //   +   - + - * +   *: true if rn not in list, false otherwise
        const bool load = bit::test<11>(opcode);
        const uint8_t regList = bit::extract<0, 8>(opcode);
        const uint8_t rn = bit::extract<8, 3>(opcode);
        instr.preindexed = false;
        instr.positiveOffset = true;
        instr.userMode = false;
        instr.writeback = !load || !bit::test(rn, regList);
        instr.load = load;
        instr.baseReg = 13;
        instr.regList = regList;

        return client.Process(instr);
    }

    template <Client TClient>
    Action SoftwareInterrupt(TClient &client) {
        return client.Process(instrs::SoftwareInterrupt{.cond = Condition::AL, .comment = bit::extract<0, 8>(opcode)});
    }

    template <Client TClient>
    Action SoftwareBreakpoint(TClient &client) {
        return client.Process(instrs::SoftwareBreakpoint{.cond = Condition::AL});
    }

    template <Client TClient>
    Action ConditionalBranch(TClient &client, uint16_t opcode) {
        instrs::Branch instr{.cond = static_cast<Condition>(bit::extract<8, 4>(opcode))};

        instr.offset = bit::sign_extend<8, int32_t>(bit::extract<0, 8>(opcode)) * 2;
        instr.link = false;
        instr.switchToThumb = false;

        return client.Process(instr);
    }

    template <Client TClient>
    Action UnconditionalBranch(TClient &client, uint16_t opcode) {
        instrs::Branch instr{.cond = Condition::AL};

        instr.offset = bit::sign_extend<11, int32_t>(bit::extract<0, 11>(opcode)) * 2;
        instr.link = false;
        instr.switchToThumb = false;

        return client.Process(instr);
    }

    template <Client TClient>
    Action LongBranchPrefix(TClient &client, uint16_t opcode) {
        instrs::DataProcessing instr{.cond = Condition::AL};

        // LR = PC + (SignExtend(offset_11) << 12)
        instr.opcode = instrs::DataProcessing::Opcode::ADD;
        instr.immediate = true;
        instr.setFlags = false;
        instr.dstReg = 14;
        instr.lhsReg = 15;
        instr.rhs.imm = bit::sign_extend<11>(bit::extract<0, 11>(opcode)) << 12;

        return client.Process(instr);
    }

    template <Client TClient>
    Action LongBranchSuffix(TClient &client, uint16_t opcode, bool blx) {
        instrs::ThumbLongBranchSuffix instr{};

        instr.offset = bit::sign_extend<11, int32_t>(bit::extract<0, 11>(opcode)) * 2;
        instr.blx = blx;

        return client.Process(instr);
    }

    template <Client TClient>
    Action Undefined(TClient &client) {
        return client.Process(instrs::Undefined{.cond = Condition::AL});
    }

} // namespace detail

template <Client TClient>
Action DecodeThumb(TClient &client, uint32_t address) {
    using namespace detail;

    const CPUArch arch = client.GetCPUArch();
    const uint16_t opcode = client.CodeReadHalf(address);

    const uint8_t group = bit::extract<12, 4>(opcode);
    switch (group) {
    case 0b0000: // fallthrough
    case 0b0001:
        if (bit::extract<11, 2>(opcode) == 0b11) {
            return AddSubRegImm(client, opcode);
        } else {
            return ShiftByImm(client, opcode);
        }
    case 0b0010: // fallthrough
    case 0b0011: return MovCmpAddSubImm(client, opcode);
    case 0b0100:
        switch (bit::extract<10, 2>(opcode)) {
        case 0b00: return DataProcessing(client, opcode);
        case 0b01:
            if (bit::extract<8, 2>(opcode) == 0b11) {
                const bool link = (arch == CPUArch::ARMv5TE) && bit::test<7>(opcode);
                return HiRegBranchExchange(client, opcode, link);
            } else {
                return HiRegOps(client, opcode);
            }
        default: return PCRelativeLoad(client, opcode);
        }
    case 0b0101: return LoadStoreRegOffset(client, opcode);
    case 0b0110: // fallthrough
    case 0b0111: return LoadStoreByteWordImmOffset(client, opcode);
    case 0b1000: return LoadStoreHalfImmOffset(client, opcode);
    case 0b1001: return SPRelativeLoadStore(client, opcode);
    case 0b1010: return AddToSPOrPC(client, opcode);
    case 0b1011:
        switch (bit::extract<8, 4>(opcode)) {
        case 0b0000: return AdjustSP(client, opcode);
        case 0b1110:
            if (arch == CPUArch::ARMv5TE) {
                return SoftwareBreakpoint(client, opcode);
            } else {
                return Undefined(client);
            }
        case 0b0100: // fallthrough
        case 0b0101: // fallthrough
        case 0b1100: // fallthrough
        case 0b1101: return PushPop(client, opcode);
        default: return Undefined(client);
        }
    case 0b1100: return LoadStoreMultiple(client, opcode);
    case 0b1101:
        switch (bit::extract<8, 4>(opcode)) {
        case 0b1110: return Undefined(client);
        case 0b1111: return SoftwareInterrupt(client, opcode);
        default: return ConditionalBranch(client, opcode);
        }
    case 0b1110: {
        if (arch == CPUArch::ARMv5TE) {
            const bool blx = bit::test<11>(opcode);
            if (blx) {
                if (bit::test<0>(opcode)) {
                    return Undefined(client);
                } else {
                    return LongBranchSuffix(client, opcode, true);
                }
            }
        }
        return UnconditionalBranch(client, opcode);
    }
    case 0b1111: {
        if (bit::test<11>(opcode)) {
            return LongBranchSuffix(client, opcode, false);
        } else {
            return LongBranchPrefix(client, opcode);
        }
    }
    }

    return Action::UnmappedInstruction;
}

} // namespace armajitto::arm::decoder
