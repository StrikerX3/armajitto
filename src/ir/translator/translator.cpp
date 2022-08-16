#include "armajitto/ir/translator.hpp"

#include "armajitto/defs/arm/instructions.hpp"
#include "decode_arm.hpp"
#include "decode_thumb.hpp"

using namespace armajitto::arm;
using namespace armajitto::arm::instrs;

namespace armajitto::ir {

template <typename FetchDecodeFn>
inline void Translator::TranslateCommon(BasicBlock &block, uint32_t startAddress, uint32_t maxBlockSize,
                                        FetchDecodeFn &&fetchDecodeFn) {
    State state{block};

    for (uint32_t i = 0; i < maxBlockSize; i++) {
        fetchDecodeFn(startAddress, i, state);

        if (state.IsEndBlock()) {
            break;
        }
        state.NextIteration();
    }
}

void Translator::TranslateARM(BasicBlock &block, uint32_t startAddress, uint32_t maxBlockSize) {
    TranslateCommon(block, startAddress, maxBlockSize, [this](uint32_t startAddress, uint32_t i, State &state) {
        uint32_t opcode = m_context.CodeReadWord(startAddress + i * sizeof(opcode));
        DecodeARM(opcode, state);
    });
}

void Translator::TranslateThumb(BasicBlock &block, uint32_t startAddress, uint32_t maxBlockSize) {
    TranslateCommon(block, startAddress, maxBlockSize, [this](uint32_t startAddress, uint32_t i, State &state) {
        uint16_t opcode = m_context.CodeReadHalf(startAddress + i * sizeof(opcode));
        DecodeThumb(opcode, state);
    });
}

void Translator::DecodeARM(uint32_t opcode, State &state) {
    const CPUArch arch = m_context.GetCPUArch();
    const auto cond = static_cast<Condition>(bit::extract<28, 4>(opcode));
    auto handle = state.GetHandle();

    if (arch == CPUArch::ARMv5TE && cond == Condition::NV) {
        // The NV condition encodes special unconditional instructions on ARMv5
        state.UpdateCondition(Condition::AL);
    } else {
        state.UpdateCondition(cond);
    }

    const uint32_t op = bit::extract<25, 3>(opcode);
    const uint32_t bits24to20 = bit::extract<20, 5>(opcode);
    const uint32_t bits7to4 = bit::extract<4, 4>(opcode);

    if (arch == CPUArch::ARMv5TE && cond == Condition::NV) {
        switch (op) {
        case 0b000: Translate(arm_decoder::Undefined(Condition::AL), handle); break;
        case 0b001: Translate(arm_decoder::Undefined(Condition::AL), handle); break;
        case 0b100: Translate(arm_decoder::Undefined(Condition::AL), handle); break;
        case 0b010: // fallthrough
        case 0b011:
            if ((bits24to20 & 0b1'0111) == 0b1'0101) {
                Translate(arm_decoder::Preload(opcode), handle);
            } else {
                Translate(arm_decoder::Undefined(Condition::AL), handle);
            }
            break;
        case 0b110: Translate(arm_decoder::CopDataTransfer(opcode, Condition::AL, true), handle); break;
        case 0b111:
            if (!bit::test<24>(opcode)) {
                if (bit::test<4>(opcode)) {
                    Translate(arm_decoder::CopRegTransfer(opcode, Condition::AL, true), handle);
                } else {
                    Translate(arm_decoder::CopDataOperations(opcode, Condition::AL, true), handle);
                }
            }
            if (bit::test<8>(opcode)) {
                Translate(arm_decoder::Undefined(Condition::AL), handle);
            }
            break;
        }
    }

    switch (op) {
    case 0b000:
        if ((bits24to20 & 0b1'1111) == 0b1'0010 && (bits7to4 & 0b1111) == 0b0001) {
            Translate(arm_decoder::BranchAndExchange(opcode, cond), handle);
        } else if ((bits24to20 & 0b1'1111) == 0b1'0010 && (bits7to4 & 0b1111) == 0b0011) {
            if (arch == CPUArch::ARMv5TE) {
                Translate(arm_decoder::BranchAndExchange(opcode, cond), handle);
            } else {
                Translate(arm_decoder::Undefined(cond), handle);
            }
        } else if ((bits24to20 & 0b1'1111) == 0b1'0110 && (bits7to4 & 0b1111) == 0b0001) {
            if (arch == CPUArch::ARMv5TE) {
                Translate(arm_decoder::CountLeadingZeros(opcode, cond), handle);
            } else {
                Translate(arm_decoder::Undefined(cond), handle);
            }
        } else if ((bits24to20 & 0b1'1111) == 0b1'0010 && (bits7to4 & 0b1111) == 0b0111) {
            if (arch == CPUArch::ARMv5TE) {
                Translate(arm_decoder::SoftwareBreakpoint(opcode, cond), handle);
            } else {
                Translate(arm_decoder::Undefined(cond), handle);
            }
        } else if ((bits24to20 & 0b1'1001) == 0b1'0000 && (bits7to4 & 0b1111) == 0b0101) {
            if (arch == CPUArch::ARMv5TE) {
                Translate(arm_decoder::SaturatingAddSub(opcode, cond), handle);
            } else {
                Translate(arm_decoder::Undefined(cond), handle);
            }
        } else if ((bits24to20 & 0b1'1001) == 0b1'0000 && (bits7to4 & 0b1001) == 0b1000) {
            if (arch == CPUArch::ARMv5TE) {
                const uint8_t op = bit::extract<21, 2>(opcode);
                switch (op) {
                case 0b00: // fallthrough
                case 0b11: Translate(arm_decoder::SignedMultiplyAccumulate(opcode, cond), handle); break;
                case 0b01: Translate(arm_decoder::SignedMultiplyAccumulateWord(opcode, cond), handle); break;
                case 0b10: Translate(arm_decoder::SignedMultiplyAccumulateLong(opcode, cond), handle); break;
                }
            } else {
                Translate(arm_decoder::Undefined(cond), handle);
            }
        } else if ((bits24to20 & 0b1'1100) == 0b0'0000 && (bits7to4 & 0b1111) == 0b1001) {
            Translate(arm_decoder::MultiplyAccumulate(opcode, cond), handle);
        } else if ((bits24to20 & 0b1'1000) == 0b0'1000 && (bits7to4 & 0b1111) == 0b1001) {
            Translate(arm_decoder::MultiplyAccumulateLong(opcode, cond), handle);
        } else if ((bits24to20 & 0b1'1011) == 0b1'0000 && (bits7to4 & 0b1111) == 0b1001) {
            Translate(arm_decoder::SingleDataSwap(opcode, cond), handle);
        } else if ((bits7to4 & 0b1001) == 0b1001) {
            const bool bit12 = bit::test<12>(opcode);
            const bool l = bit::test<20>(opcode);
            const bool s = bit::test<6>(opcode);
            const bool h = bit::test<5>(opcode);
            if (l) {
                Translate(arm_decoder::HalfwordAndSignedTransfer(opcode, cond), handle);
            } else {
                if (s && h) {
                    if (arch == CPUArch::ARMv5TE) {
                        if (bit12) {
                            Translate(arm_decoder::Undefined(cond), handle);
                        } else {
                            Translate(arm_decoder::HalfwordAndSignedTransfer(opcode, cond), handle);
                        }
                    }
                } else if (s) {
                    if (arch == CPUArch::ARMv5TE) {
                        if (bit12) {
                            Translate(arm_decoder::Undefined(cond), handle);
                        } else {
                            Translate(arm_decoder::HalfwordAndSignedTransfer(opcode, cond), handle);
                        }
                    }
                } else if (h) {
                    Translate(arm_decoder::HalfwordAndSignedTransfer(opcode, cond), handle);
                }
            }
        } else if ((bits24to20 & 0b1'1011) == 0b1'0000 && (bits7to4 & 0b1111) == 0b0000) {
            Translate(arm_decoder::PSRRead(opcode, cond), handle);
        } else if ((bits24to20 & 0b1'1011) == 0b1'0010 && (bits7to4 & 0b1111) == 0b0000) {
            Translate(arm_decoder::PSRWrite(opcode, cond), handle);
        } else {
            Translate(arm_decoder::DataProcessing(opcode, cond), handle);
        }
        break;
    case 0b001:
        if ((bits24to20 & 0b1'1011) == 0b1'0010) {
            Translate(arm_decoder::PSRWrite(opcode, cond), handle);
        } else if ((bits24to20 & 0b1'1011) == 0b1'0000) {
            Translate(arm_decoder::Undefined(cond), handle);
        } else {
            Translate(arm_decoder::DataProcessing(opcode, cond), handle);
        }
        break;
    case 0b010:
    case 0b011:
        if (bit::test<0>(op) && bit::test<0>(bits7to4)) {
            Translate(arm_decoder::Undefined(cond), handle);
        } else {
            Translate(arm_decoder::SingleDataTransfer(opcode, cond), handle);
        }
        break;
    case 0b100: Translate(arm_decoder::BlockTransfer(opcode, cond), handle); break;
    case 0b101: {
        const bool switchToThumb = (arch == CPUArch::ARMv5TE) && (cond == Condition::NV);
        Translate(arm_decoder::Branch(opcode, cond, switchToThumb), handle);
        break;
    }
    case 0b110:
        if (arch == CPUArch::ARMv5TE) {
            if ((bits24to20 & 0b1'1110) == 0b0'0100) {
                Translate(arm_decoder::CopDualRegTransfer(opcode, cond), handle);
            }
        }
        Translate(arm_decoder::CopDataTransfer(opcode, cond, false), handle);
        break;
    case 0b111: {
        if (bit::test<24>(opcode)) {
            Translate(arm_decoder::SoftwareInterrupt(opcode, cond), handle);
        } else {
            if (bit::test<4>(opcode)) {
                Translate(arm_decoder::CopRegTransfer(opcode, cond, false), handle);
            } else {
                Translate(arm_decoder::CopDataOperations(opcode, cond, false), handle);
            }
        }
        break;
    }
    }
}

void Translator::DecodeThumb(uint16_t opcode, State &state) {
    const CPUArch arch = m_context.GetCPUArch();
    auto handle = state.GetHandle();

    const uint8_t group = bit::extract<12, 4>(opcode);

    if (group == 0b1101) {
        // Parse Thumb conditional branch
        auto cond = static_cast<arm::Condition>(bit::extract<8, 4>(opcode));
        if (cond == Condition::NV) {
            // 0b1111 (Condition::NV) is a software interrupt instruction
            state.UpdateCondition(Condition::AL);
        } else {
            // 0b1110 (Condition::AL) is an undefined instruction
            // All other combinations are valid conditions
            state.UpdateCondition(cond);
        }
    } else {
        state.UpdateCondition(Condition::AL);
    }

    switch (group) {
    case 0b0000: // fallthrough
    case 0b0001:
        if (bit::extract<11, 2>(opcode) == 0b11) {
            Translate(thumb_decoder::AddSubRegImm(opcode), handle);
        } else {
            Translate(thumb_decoder::ShiftByImm(opcode), handle);
        }
        break;
    case 0b0010: // fallthrough
    case 0b0011: Translate(thumb_decoder::MovCmpAddSubImm(opcode), handle); break;
    case 0b0100:
        switch (bit::extract<10, 2>(opcode)) {
        case 0b00: {
            using DPOpcode = instrs::DataProcessing::Opcode;

            const auto op = bit::extract<6, 4>(opcode);
            switch (op) {
            case 0x0 /*AND*/: Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::AND), handle); break;
            case 0x1 /*EOR*/: Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::EOR), handle); break;
            case 0x2 /*LSL*/: Translate(thumb_decoder::DataProcessingShift(opcode, ShiftType::LSL), handle); break;
            case 0x3 /*LSR*/: Translate(thumb_decoder::DataProcessingShift(opcode, ShiftType::LSR), handle); break;
            case 0x4 /*ASR*/: Translate(thumb_decoder::DataProcessingShift(opcode, ShiftType::ASR), handle); break;
            case 0x5 /*ADC*/: Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::ADC), handle); break;
            case 0x6 /*SBC*/: Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::SBC), handle); break;
            case 0x7 /*ROR*/: Translate(thumb_decoder::DataProcessingShift(opcode, ShiftType::ROR), handle); break;
            case 0x8 /*TST*/: Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::TST), handle); break;
            case 0x9 /*NEG*/: Translate(thumb_decoder::DataProcessingNegate(opcode), handle); break;
            case 0xA /*CMP*/: Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::CMP), handle); break;
            case 0xB /*CMN*/: Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::CMN), handle); break;
            case 0xC /*ORR*/: Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::ORR), handle); break;
            case 0xD /*MUL*/: Translate(thumb_decoder::DataProcessingMultiply(opcode), handle); break;
            case 0xE /*BIC*/: Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::BIC), handle); break;
            case 0xF /*MVN*/: Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::MVN), handle); break;
            }
            break;
        }
        case 0b01:
            if (bit::extract<8, 2>(opcode) == 0b11) {
                const bool link = (arch == CPUArch::ARMv5TE) && bit::test<7>(opcode);
                Translate(thumb_decoder::HiRegBranchExchange(opcode, link), handle);
            } else {
                Translate(thumb_decoder::HiRegOps(opcode), handle);
            }
            break;
        default: Translate(thumb_decoder::PCRelativeLoad(opcode), handle); break;
        }
        break;
    case 0b0101:
        if (bit::test<9>(opcode)) {
            Translate(thumb_decoder::LoadStoreHalfRegOffset(opcode), handle);
        } else {
            Translate(thumb_decoder::LoadStoreByteWordRegOffset(opcode), handle);
        }
        break;
    case 0b0110: // fallthrough
    case 0b0111: Translate(thumb_decoder::LoadStoreByteWordImmOffset(opcode), handle); break;
    case 0b1000: Translate(thumb_decoder::LoadStoreHalfImmOffset(opcode), handle); break;
    case 0b1001: Translate(thumb_decoder::SPRelativeLoadStore(opcode), handle); break;
    case 0b1010: Translate(thumb_decoder::AddToSPOrPC(opcode), handle); break;
    case 0b1011:
        switch (bit::extract<8, 4>(opcode)) {
        case 0b0000: Translate(thumb_decoder::AdjustSP(opcode), handle); break;
        case 0b1110:
            if (arch == CPUArch::ARMv5TE) {
                Translate(thumb_decoder::SoftwareBreakpoint(), handle);
            } else {
                Translate(thumb_decoder::Undefined(), handle);
            }
            break;
        case 0b0100: // fallthrough
        case 0b0101: // fallthrough
        case 0b1100: // fallthrough
        case 0b1101: Translate(thumb_decoder::PushPop(opcode), handle); break;
        default: Translate(thumb_decoder::Undefined(), handle); break;
        }
        break;
    case 0b1100: Translate(thumb_decoder::LoadStoreMultiple(opcode), handle); break;
    case 0b1101:
        switch (bit::extract<8, 4>(opcode)) {
        case 0b1110: Translate(thumb_decoder::Undefined(), handle); break;
        case 0b1111: Translate(thumb_decoder::SoftwareInterrupt(opcode), handle); break;
        default: Translate(thumb_decoder::ConditionalBranch(opcode), handle); break;
        }
        break;
    case 0b1110:
        if (arch == CPUArch::ARMv5TE) {
            const bool blx = bit::test<11>(opcode);
            if (blx) {
                if (bit::test<0>(opcode)) {
                    Translate(thumb_decoder::Undefined(), handle);
                } else {
                    Translate(thumb_decoder::LongBranchSuffix(opcode, true), handle);
                }
            }
        }
        Translate(thumb_decoder::UnconditionalBranch(opcode), handle);
        break;
    case 0b1111:
        if (bit::test<11>(opcode)) {
            Translate(thumb_decoder::LongBranchSuffix(opcode, false), handle);
        } else {
            Translate(thumb_decoder::LongBranchPrefix(opcode), handle);
        }
        break;
    }
}

void Translator::Translate(const Branch &instr, State::Handle state) {
    // TODO: implement
}

void Translator::Translate(const BranchAndExchange &instr, State::Handle state) {
    // TODO: implement
}

void Translator::Translate(const ThumbLongBranchSuffix &instr, State::Handle state) {
    // TODO: implement
}

void Translator::Translate(const DataProcessing &instr, State::Handle state) {
    // TODO: implement
}

void Translator::Translate(const CountLeadingZeros &instr, State::Handle state) {
    auto &emitter = state.GetEmitter();
    auto argVar = emitter.CreateVariable("arg");
    auto dstVar = emitter.CreateVariable("dst");

    emitter.LoadGPR(argVar, instr.argReg);
    emitter.CountLeadingZeros(dstVar, argVar);
    emitter.StoreGPR(instr.dstReg, dstVar);
}

void Translator::Translate(const SaturatingAddSub &instr, State::Handle state) {
    // TODO: implement
}

void Translator::Translate(const MultiplyAccumulate &instr, State::Handle state) {
    // TODO: implement
}

void Translator::Translate(const MultiplyAccumulateLong &instr, State::Handle state) {
    // TODO: implement
}

void Translator::Translate(const SignedMultiplyAccumulate &instr, State::Handle state) {
    // TODO: implement
}

void Translator::Translate(const SignedMultiplyAccumulateWord &instr, State::Handle state) {
    // TODO: implement
}

void Translator::Translate(const SignedMultiplyAccumulateLong &instr, State::Handle state) {
    // TODO: implement
}

void Translator::Translate(const PSRRead &instr, State::Handle state) {
    // TODO: implement
}

void Translator::Translate(const PSRWrite &instr, State::Handle state) {
    // TODO: implement
}

void Translator::Translate(const SingleDataTransfer &instr, State::Handle state) {
    // TODO: implement
}

void Translator::Translate(const HalfwordAndSignedTransfer &instr, State::Handle state) {
    // TODO: implement
}

void Translator::Translate(const BlockTransfer &instr, State::Handle state) {
    // TODO: implement
}

void Translator::Translate(const SingleDataSwap &instr, State::Handle state) {
    // TODO: implement
}

void Translator::Translate(const SoftwareInterrupt &instr, State::Handle state) {
    // TODO: implement
}

void Translator::Translate(const SoftwareBreakpoint &instr, State::Handle state) {
    // TODO: implement
}

void Translator::Translate(const Preload &instr, State::Handle state) {
    // TODO: implement
}

void Translator::Translate(const CopDataOperations &instr, State::Handle state) {
    // TODO: implement
}

void Translator::Translate(const CopDataTransfer &instr, State::Handle state) {
    // TODO: implement
}

void Translator::Translate(const CopRegTransfer &instr, State::Handle state) {
    // TODO: implement
}

void Translator::Translate(const CopDualRegTransfer &instr, State::Handle state) {
    // TODO: implement
}

void Translator::Translate(const Undefined &instr, State::Handle state) {
    // TODO: implement
}

} // namespace armajitto::ir
