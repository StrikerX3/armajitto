#include "armajitto/ir/translator.hpp"

#include "armajitto/defs/arm/instructions.hpp"
#include "decode_arm.hpp"
#include "decode_thumb.hpp"

using namespace armajitto::arm;
using namespace armajitto::arm::instrs;

namespace armajitto::ir {

template <typename DecodeFn>
inline void Translator::TranslateCommon(BasicBlock &block, uint32_t startAddress, uint32_t maxBlockSize,
                                        DecodeFn &&decodeFn) {
    auto *codeFrag = block.CreateCodeFragment();

    for (uint32_t i = 0; i < maxBlockSize; i++) {
        auto action = decodeFn(startAddress, i, codeFrag->emitter);
        if (action == Action::End) {
            break;
        }
        switch (action) {
        case Action::Split: codeFrag = block.CreateCodeFragment(); break;
        case Action::Unimplemented: break;       // TODO: throw exception or report error somehow
        case Action::UnmappedInstruction: break; // TODO: throw exception or report error somehow
        default: break;                          // TODO: unreachable
        }
    }
}

void Translator::TranslateARM(BasicBlock &block, uint32_t startAddress, uint32_t maxBlockSize) {
    TranslateCommon(block, startAddress, maxBlockSize, [this](uint32_t startAddress, uint32_t i, Emitter &emitter) {
        uint32_t opcode = m_context.CodeReadWord(startAddress + i * 4);
        return DecodeARM(opcode, emitter);
    });
}

void Translator::TranslateThumb(BasicBlock &block, uint32_t startAddress, uint32_t maxBlockSize) {
    TranslateCommon(block, startAddress, maxBlockSize, [this](uint32_t startAddress, uint32_t i, Emitter &emitter) {
        uint16_t opcode = m_context.CodeReadHalf(startAddress + i * 2);
        return DecodeThumb(opcode, emitter);
    });
}

auto Translator::DecodeARM(uint32_t opcode, Emitter &emitter) -> Action {
    const CPUArch arch = m_context.GetCPUArch();
    const auto cond = static_cast<Condition>(bit::extract<28, 4>(opcode));

    const uint32_t op = bit::extract<25, 3>(opcode);
    const uint32_t bits24to20 = bit::extract<20, 5>(opcode);
    const uint32_t bits7to4 = bit::extract<4, 4>(opcode);

    if (arch == CPUArch::ARMv5TE) {
        if (cond == Condition::NV) {
            switch (op) {
            case 0b000: return Translate(arm_decoder::Undefined(Condition::AL), emitter);
            case 0b001: return Translate(arm_decoder::Undefined(Condition::AL), emitter);
            case 0b100: return Translate(arm_decoder::Undefined(Condition::AL), emitter);
            case 0b010: // fallthrough
            case 0b011:
                if ((bits24to20 & 0b1'0111) == 0b1'0101) {
                    return Translate(arm_decoder::Preload(opcode), emitter);
                } else {
                    return Translate(arm_decoder::Undefined(cond), emitter);
                }
            case 0b110: return Translate(arm_decoder::CopDataTransfer(opcode, Condition::AL, true), emitter);
            case 0b111:
                if (!bit::test<24>(opcode)) {
                    if (bit::test<4>(opcode)) {
                        return Translate(arm_decoder::CopRegTransfer(opcode, Condition::AL, true), emitter);
                    } else {
                        return Translate(arm_decoder::CopDataOperations(opcode, Condition::AL, true), emitter);
                    }
                }
                if (bit::test<8>(opcode)) {
                    return Translate(arm_decoder::Undefined(cond), emitter);
                }
                break;
            }
        }
    }

    switch (op) {
    case 0b000:
        if ((bits24to20 & 0b1'1111) == 0b1'0010 && (bits7to4 & 0b1111) == 0b0001) {
            return Translate(arm_decoder::BranchAndExchange(opcode, cond), emitter);
        } else if ((bits24to20 & 0b1'1111) == 0b1'0010 && (bits7to4 & 0b1111) == 0b0011) {
            if (arch == CPUArch::ARMv5TE) {
                return Translate(arm_decoder::BranchAndExchange(opcode, cond), emitter);
            } else {
                return Translate(arm_decoder::Undefined(cond), emitter);
            }
        } else if ((bits24to20 & 0b1'1111) == 0b1'0110 && (bits7to4 & 0b1111) == 0b0001) {
            if (arch == CPUArch::ARMv5TE) {
                return Translate(arm_decoder::CountLeadingZeros(opcode, cond), emitter);
            } else {
                return Translate(arm_decoder::Undefined(cond), emitter);
            }
        } else if ((bits24to20 & 0b1'1111) == 0b1'0010 && (bits7to4 & 0b1111) == 0b0111) {
            if (arch == CPUArch::ARMv5TE) {
                return Translate(arm_decoder::SoftwareBreakpoint(opcode, cond), emitter);
            } else {
                return Translate(arm_decoder::Undefined(cond), emitter);
            }
        } else if ((bits24to20 & 0b1'1001) == 0b1'0000 && (bits7to4 & 0b1111) == 0b0101) {
            if (arch == CPUArch::ARMv5TE) {
                return Translate(arm_decoder::SaturatingAddSub(opcode, cond), emitter);
            } else {
                return Translate(arm_decoder::Undefined(cond), emitter);
            }
        } else if ((bits24to20 & 0b1'1001) == 0b1'0000 && (bits7to4 & 0b1001) == 0b1000) {
            if (arch == CPUArch::ARMv5TE) {
                const uint8_t op = bit::extract<21, 2>(opcode);
                switch (op) {
                case 0b00:
                case 0b11: return Translate(arm_decoder::SignedMultiplyAccumulate(opcode, cond), emitter);
                case 0b01: return Translate(arm_decoder::SignedMultiplyAccumulateWord(opcode, cond), emitter);
                case 0b10: return Translate(arm_decoder::SignedMultiplyAccumulateLong(opcode, cond), emitter);
                }
            } else {
                return Translate(arm_decoder::Undefined(cond), emitter);
            }
        } else if ((bits24to20 & 0b1'1100) == 0b0'0000 && (bits7to4 & 0b1111) == 0b1001) {
            return Translate(arm_decoder::MultiplyAccumulate(opcode, cond), emitter);
        } else if ((bits24to20 & 0b1'1000) == 0b0'1000 && (bits7to4 & 0b1111) == 0b1001) {
            return Translate(arm_decoder::MultiplyAccumulateLong(opcode, cond), emitter);
        } else if ((bits24to20 & 0b1'1011) == 0b1'0000 && (bits7to4 & 0b1111) == 0b1001) {
            return Translate(arm_decoder::SingleDataSwap(opcode, cond), emitter);
        } else if ((bits7to4 & 0b1001) == 0b1001) {
            const bool bit12 = bit::test<12>(opcode);
            const bool l = bit::test<20>(opcode);
            const bool s = bit::test<6>(opcode);
            const bool h = bit::test<5>(opcode);
            if (l) {
                return Translate(arm_decoder::HalfwordAndSignedTransfer(opcode, cond), emitter);
            } else {
                if (s && h) {
                    if (arch == CPUArch::ARMv5TE) {
                        if (bit12) {
                            return Translate(arm_decoder::Undefined(cond), emitter);
                        } else {
                            return Translate(arm_decoder::HalfwordAndSignedTransfer(opcode, cond), emitter);
                        }
                    }
                } else if (s) {
                    if (arch == CPUArch::ARMv5TE) {
                        if (bit12) {
                            return Translate(arm_decoder::Undefined(cond), emitter);
                        } else {
                            return Translate(arm_decoder::HalfwordAndSignedTransfer(opcode, cond), emitter);
                        }
                    }
                } else if (h) {
                    return Translate(arm_decoder::HalfwordAndSignedTransfer(opcode, cond), emitter);
                }
            }
        } else if ((bits24to20 & 0b1'1011) == 0b1'0000 && (bits7to4 & 0b1111) == 0b0000) {
            return Translate(arm_decoder::PSRRead(opcode, cond), emitter);
        } else if ((bits24to20 & 0b1'1011) == 0b1'0010 && (bits7to4 & 0b1111) == 0b0000) {
            return Translate(arm_decoder::PSRWrite(opcode, cond), emitter);
        } else {
            return Translate(arm_decoder::DataProcessing(opcode, cond), emitter);
        }
        break;
    case 0b001:
        if ((bits24to20 & 0b1'1011) == 0b1'0010) {
            return Translate(arm_decoder::PSRWrite(opcode, cond), emitter);
        } else if ((bits24to20 & 0b1'1011) == 0b1'0000) {
            return Translate(arm_decoder::Undefined(cond), emitter);
        } else {
            return Translate(arm_decoder::DataProcessing(opcode, cond), emitter);
        }
        break;
    case 0b010:
    case 0b011:
        if (bit::test<0>(op) && bit::test<0>(bits7to4)) {
            return Translate(arm_decoder::Undefined(cond), emitter);
        } else {
            return Translate(arm_decoder::SingleDataTransfer(opcode, cond), emitter);
        }
        break;
    case 0b100: return Translate(arm_decoder::BlockTransfer(opcode, cond), emitter);
    case 0b101: {
        const bool switchToThumb = (arch == CPUArch::ARMv5TE) && (cond == Condition::NV);
        return Translate(arm_decoder::Branch(opcode, cond, switchToThumb), emitter);
    }
    case 0b110:
        if (arch == CPUArch::ARMv5TE) {
            if ((bits24to20 & 0b1'1110) == 0b0'0100) {
                return Translate(arm_decoder::CopDualRegTransfer(opcode, cond), emitter);
            }
        }
        return Translate(arm_decoder::CopDataTransfer(opcode, cond, false), emitter);
    case 0b111: {
        if (bit::test<24>(opcode)) {
            return Translate(arm_decoder::SoftwareInterrupt(opcode, cond), emitter);
        } else {
            if (bit::test<4>(opcode)) {
                return Translate(arm_decoder::CopRegTransfer(opcode, cond, false), emitter);
            } else {
                return Translate(arm_decoder::CopDataOperations(opcode, cond, false), emitter);
            }
        }
        break;
    }
    }

    return Action::UnmappedInstruction;
}

auto Translator::DecodeThumb(uint16_t opcode, Emitter &emitter) -> Action {
    const CPUArch arch = m_context.GetCPUArch();

    const uint8_t group = bit::extract<12, 4>(opcode);
    switch (group) {
    case 0b0000: // fallthrough
    case 0b0001:
        if (bit::extract<11, 2>(opcode) == 0b11) {
            return Translate(thumb_decoder::AddSubRegImm(opcode), emitter);
        } else {
            return Translate(thumb_decoder::ShiftByImm(opcode), emitter);
        }
    case 0b0010: // fallthrough
    case 0b0011: return Translate(thumb_decoder::MovCmpAddSubImm(opcode), emitter);
    case 0b0100:
        switch (bit::extract<10, 2>(opcode)) {
        case 0b00: {
            using DPOpcode = instrs::DataProcessing::Opcode;

            const auto op = bit::extract<6, 4>(opcode);
            switch (op) {
            case 0x0 /*AND*/: return Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::AND), emitter);
            case 0x1 /*EOR*/: return Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::EOR), emitter);
            case 0x2 /*LSL*/: return Translate(thumb_decoder::DataProcessingShift(opcode, ShiftType::LSL), emitter);
            case 0x3 /*LSR*/: return Translate(thumb_decoder::DataProcessingShift(opcode, ShiftType::LSR), emitter);
            case 0x4 /*ASR*/: return Translate(thumb_decoder::DataProcessingShift(opcode, ShiftType::ASR), emitter);
            case 0x5 /*ADC*/: return Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::ADC), emitter);
            case 0x6 /*SBC*/: return Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::SBC), emitter);
            case 0x7 /*ROR*/: return Translate(thumb_decoder::DataProcessingShift(opcode, ShiftType::ROR), emitter);
            case 0x8 /*TST*/: return Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::TST), emitter);
            case 0x9 /*NEG*/: return Translate(thumb_decoder::DataProcessingNegate(opcode), emitter);
            case 0xA /*CMP*/: return Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::CMP), emitter);
            case 0xB /*CMN*/: return Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::CMN), emitter);
            case 0xC /*ORR*/: return Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::ORR), emitter);
            case 0xD /*MUL*/: return Translate(thumb_decoder::DataProcessingMultiply(opcode), emitter);
            case 0xE /*BIC*/: return Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::BIC), emitter);
            case 0xF /*MVN*/: return Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::MVN), emitter);
            }
        }
        case 0b01:
            if (bit::extract<8, 2>(opcode) == 0b11) {
                const bool link = (arch == CPUArch::ARMv5TE) && bit::test<7>(opcode);
                return Translate(thumb_decoder::HiRegBranchExchange(opcode, link), emitter);
            } else {
                return Translate(thumb_decoder::HiRegOps(opcode), emitter);
            }
        default: return Translate(thumb_decoder::PCRelativeLoad(opcode), emitter);
        }
    case 0b0101:
        if (bit::test<9>(opcode)) {
            return Translate(thumb_decoder::LoadStoreHalfRegOffset(opcode), emitter);
        } else {
            return Translate(thumb_decoder::LoadStoreByteWordRegOffset(opcode), emitter);
        }
    case 0b0110: // fallthrough
    case 0b0111: return Translate(thumb_decoder::LoadStoreByteWordImmOffset(opcode), emitter);
    case 0b1000: return Translate(thumb_decoder::LoadStoreHalfImmOffset(opcode), emitter);
    case 0b1001: return Translate(thumb_decoder::SPRelativeLoadStore(opcode), emitter);
    case 0b1010: return Translate(thumb_decoder::AddToSPOrPC(opcode), emitter);
    case 0b1011:
        switch (bit::extract<8, 4>(opcode)) {
        case 0b0000: return Translate(thumb_decoder::AdjustSP(opcode), emitter);
        case 0b1110:
            if (arch == CPUArch::ARMv5TE) {
                return Translate(thumb_decoder::SoftwareBreakpoint(), emitter);
            } else {
                return Translate(thumb_decoder::Undefined(), emitter);
            }
        case 0b0100: // fallthrough
        case 0b0101: // fallthrough
        case 0b1100: // fallthrough
        case 0b1101: return Translate(thumb_decoder::PushPop(opcode), emitter);
        default: return Translate(thumb_decoder::Undefined(), emitter);
        }
    case 0b1100: return Translate(thumb_decoder::LoadStoreMultiple(opcode), emitter);
    case 0b1101:
        switch (bit::extract<8, 4>(opcode)) {
        case 0b1110: return Translate(thumb_decoder::Undefined(), emitter);
        case 0b1111: return Translate(thumb_decoder::SoftwareInterrupt(opcode), emitter);
        default: return Translate(thumb_decoder::ConditionalBranch(opcode), emitter);
        }
    case 0b1110:
        if (arch == CPUArch::ARMv5TE) {
            const bool blx = bit::test<11>(opcode);
            if (blx) {
                if (bit::test<0>(opcode)) {
                    return Translate(thumb_decoder::Undefined(), emitter);
                } else {
                    return Translate(thumb_decoder::LongBranchSuffix(opcode, true), emitter);
                }
            }
        }
        return Translate(thumb_decoder::UnconditionalBranch(opcode), emitter);
    case 0b1111:
        if (bit::test<11>(opcode)) {
            return Translate(thumb_decoder::LongBranchSuffix(opcode, false), emitter);
        } else {
            return Translate(thumb_decoder::LongBranchPrefix(opcode), emitter);
        }
    }

    return Action::UnmappedInstruction;
}

auto Translator::Translate(const Branch &instr, Emitter &emitter) -> Action {
    // TODO: implement
    return Action::Unimplemented;
}

auto Translator::Translate(const BranchAndExchange &instr, Emitter &emitter) -> Action {
    // TODO: implement
    return Action::Unimplemented;
}

auto Translator::Translate(const ThumbLongBranchSuffix &instr, Emitter &emitter) -> Action {
    // TODO: implement
    return Action::Unimplemented;
}

auto Translator::Translate(const DataProcessing &instr, Emitter &emitter) -> Action {
    // TODO: implement
    return Action::Unimplemented;
}

auto Translator::Translate(const CountLeadingZeros &instr, Emitter &emitter) -> Action {
    auto argVar = emitter.CreateVariable("arg");
    auto dstVar = emitter.CreateVariable("dst");

    emitter.EmitLoadGPR(argVar, instr.argReg);
    emitter.EmitCountLeadingZeros(dstVar, argVar);
    emitter.EmitStoreGPR(instr.dstReg, dstVar);

    return Action::Continue;
}

auto Translator::Translate(const SaturatingAddSub &instr, Emitter &emitter) -> Action {
    // TODO: implement
    return Action::Unimplemented;
}

auto Translator::Translate(const MultiplyAccumulate &instr, Emitter &emitter) -> Action {
    // TODO: implement
    return Action::Unimplemented;
}

auto Translator::Translate(const MultiplyAccumulateLong &instr, Emitter &emitter) -> Action {
    // TODO: implement
    return Action::Unimplemented;
}

auto Translator::Translate(const SignedMultiplyAccumulate &instr, Emitter &emitter) -> Action {
    // TODO: implement
    return Action::Unimplemented;
}

auto Translator::Translate(const SignedMultiplyAccumulateWord &instr, Emitter &emitter) -> Action {
    // TODO: implement
    return Action::Unimplemented;
}

auto Translator::Translate(const SignedMultiplyAccumulateLong &instr, Emitter &emitter) -> Action {
    // TODO: implement
    return Action::Unimplemented;
}

auto Translator::Translate(const PSRRead &instr, Emitter &emitter) -> Action {
    // TODO: implement
    return Action::Unimplemented;
}

auto Translator::Translate(const PSRWrite &instr, Emitter &emitter) -> Action {
    // TODO: implement
    return Action::Unimplemented;
}

auto Translator::Translate(const SingleDataTransfer &instr, Emitter &emitter) -> Action {
    // TODO: implement
    return Action::Unimplemented;
}

auto Translator::Translate(const HalfwordAndSignedTransfer &instr, Emitter &emitter) -> Action {
    // TODO: implement
    return Action::Unimplemented;
}

auto Translator::Translate(const BlockTransfer &instr, Emitter &emitter) -> Action {
    // TODO: implement
    return Action::Unimplemented;
}

auto Translator::Translate(const SingleDataSwap &instr, Emitter &emitter) -> Action {
    // TODO: implement
    return Action::Unimplemented;
}

auto Translator::Translate(const SoftwareInterrupt &instr, Emitter &emitter) -> Action {
    // TODO: implement
    return Action::Unimplemented;
}

auto Translator::Translate(const SoftwareBreakpoint &instr, Emitter &emitter) -> Action {
    // TODO: implement
    return Action::Unimplemented;
}

auto Translator::Translate(const Preload &instr, Emitter &emitter) -> Action {
    // TODO: implement
    return Action::Unimplemented;
}

auto Translator::Translate(const CopDataOperations &instr, Emitter &emitter) -> Action {
    // TODO: implement
    return Action::Unimplemented;
}

auto Translator::Translate(const CopDataTransfer &instr, Emitter &emitter) -> Action {
    // TODO: implement
    return Action::Unimplemented;
}

auto Translator::Translate(const CopRegTransfer &instr, Emitter &emitter) -> Action {
    // TODO: implement
    return Action::Unimplemented;
}

auto Translator::Translate(const CopDualRegTransfer &instr, Emitter &emitter) -> Action {
    // TODO: implement
    return Action::Unimplemented;
}

auto Translator::Translate(const Undefined &instr, Emitter &emitter) -> Action {
    // TODO: implement
    return Action::Unimplemented;
}

} // namespace armajitto::ir
