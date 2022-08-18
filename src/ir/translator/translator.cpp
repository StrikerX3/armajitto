#include "armajitto/ir/translator.hpp"

#include "armajitto/guest/arm/instructions.hpp"
#include "decode_arm.hpp"
#include "decode_thumb.hpp"

#include <bit>

using namespace armajitto::arm;
using namespace armajitto::arm::instrs;

namespace armajitto::ir {

void Translator::Translate(BasicBlock &block) {
    Emitter emitter{block};

    m_flagsUpdated = false;
    m_endBlock = false;

    const bool thumb = block.Location().IsThumbMode();
    const uint32_t opcodeSize = thumb ? sizeof(uint16_t) : sizeof(uint32_t);
    const CPUArch arch = m_context.GetCPUArch();

    auto parseARMCond = [](uint32_t opcode, CPUArch arch) {
        const auto cond = static_cast<Condition>(bit::extract<28, 4>(opcode));
        if (arch == CPUArch::ARMv5TE && cond == Condition::NV) {
            // The NV condition encodes special unconditional instructions on ARMv5
            return Condition::AL;
        } else {
            return cond;
        }
    };

    auto parseThumbCond = [](uint16_t opcode) {
        const uint8_t group = bit::extract<12, 4>(opcode);
        if (group == 0b1101) {
            // Thumb conditional branch instruction
            auto cond = static_cast<arm::Condition>(bit::extract<8, 4>(opcode));
            if (cond == Condition::NV) {
                // 0b1111 (Condition::NV) is an unconditional software interrupt instruction
                return Condition::AL;
            } else {
                // 0b1110 (Condition::AL) is an unconditional undefined instruction
                return cond;
            }
        } else {
            return Condition::AL;
        }
    };

    uint32_t address = block.Location().BaseAddress();
    for (uint32_t i = 0; i < m_params.maxBlockSize; i++) {
        if (thumb) {
            const uint16_t opcode = m_context.CodeReadHalf(address);
            const Condition cond = parseThumbCond(opcode);
            if (i == 0) {
                emitter.SetCondition(cond);
            } else if (cond != block.Condition()) {
                break;
            }
            TranslateThumb(opcode, emitter);
        } else {
            const uint32_t opcode = m_context.CodeReadWord(address);
            const Condition cond = parseARMCond(opcode, arch);
            if (i == 0) {
                emitter.SetCondition(cond);
            } else if (cond != block.Condition()) {
                break;
            }
            TranslateARM(opcode, emitter);
        }

        emitter.NextInstruction();
        if (m_flagsUpdated && block.Condition() != arm::Condition::AL) {
            break;
        }
        if (m_endBlock) {
            break;
        }
        m_flagsUpdated = false;

        address += opcodeSize;
    }
}

void Translator::TranslateARM(uint32_t opcode, Emitter &emitter) {
    const CPUArch arch = m_context.GetCPUArch();
    const auto cond = static_cast<Condition>(bit::extract<28, 4>(opcode));

    const uint32_t op = bit::extract<25, 3>(opcode);
    const uint32_t bits24to20 = bit::extract<20, 5>(opcode);
    const uint32_t bits7to4 = bit::extract<4, 4>(opcode);

    const bool extendedARMv5TEInstr = (arch == CPUArch::ARMv5TE && cond == Condition::NV);

    switch (op) {
    case 0b000:
        if (extendedARMv5TEInstr) {
            Translate(arm_decoder::Undefined(), emitter);
        } else if ((bits24to20 & 0b1'1111) == 0b1'0010 && (bits7to4 & 0b1111) == 0b0001) {
            Translate(arm_decoder::BranchExchangeRegister(opcode), emitter);
        } else if ((bits24to20 & 0b1'1111) == 0b1'0010 && (bits7to4 & 0b1111) == 0b0011) {
            if (arch == CPUArch::ARMv5TE) {
                Translate(arm_decoder::BranchExchangeRegister(opcode), emitter);
            } else {
                Translate(arm_decoder::Undefined(), emitter);
            }
        } else if ((bits24to20 & 0b1'1111) == 0b1'0110 && (bits7to4 & 0b1111) == 0b0001) {
            if (arch == CPUArch::ARMv5TE) {
                Translate(arm_decoder::CountLeadingZeros(opcode), emitter);
            } else {
                Translate(arm_decoder::Undefined(), emitter);
            }
        } else if ((bits24to20 & 0b1'1111) == 0b1'0010 && (bits7to4 & 0b1111) == 0b0111) {
            if (arch == CPUArch::ARMv5TE) {
                Translate(arm_decoder::SoftwareBreakpoint(opcode), emitter);
            } else {
                Translate(arm_decoder::Undefined(), emitter);
            }
        } else if ((bits24to20 & 0b1'1001) == 0b1'0000 && (bits7to4 & 0b1111) == 0b0101) {
            if (arch == CPUArch::ARMv5TE) {
                Translate(arm_decoder::SaturatingAddSub(opcode), emitter);
            } else {
                Translate(arm_decoder::Undefined(), emitter);
            }
        } else if ((bits24to20 & 0b1'1001) == 0b1'0000 && (bits7to4 & 0b1001) == 0b1000) {
            if (arch == CPUArch::ARMv5TE) {
                const uint8_t op = bit::extract<21, 2>(opcode);
                switch (op) {
                case 0b00: [[fallthrough]];
                case 0b11: Translate(arm_decoder::SignedMultiplyAccumulate(opcode), emitter); break;
                case 0b01: Translate(arm_decoder::SignedMultiplyAccumulateWord(opcode), emitter); break;
                case 0b10: Translate(arm_decoder::SignedMultiplyAccumulateLong(opcode), emitter); break;
                }
            } else {
                Translate(arm_decoder::Undefined(), emitter);
            }
        } else if ((bits24to20 & 0b1'1100) == 0b0'0000 && (bits7to4 & 0b1111) == 0b1001) {
            Translate(arm_decoder::MultiplyAccumulate(opcode), emitter);
        } else if ((bits24to20 & 0b1'1000) == 0b0'1000 && (bits7to4 & 0b1111) == 0b1001) {
            Translate(arm_decoder::MultiplyAccumulateLong(opcode), emitter);
        } else if ((bits24to20 & 0b1'1011) == 0b1'0000 && (bits7to4 & 0b1111) == 0b1001) {
            Translate(arm_decoder::SingleDataSwap(opcode), emitter);
        } else if ((bits7to4 & 0b1001) == 0b1001) {
            const bool bit12 = bit::test<12>(opcode);
            const bool l = bit::test<20>(opcode);
            const bool s = bit::test<6>(opcode);
            const bool h = bit::test<5>(opcode);
            if (l) {
                Translate(arm_decoder::HalfwordAndSignedTransfer(opcode), emitter);
            } else if (s && h) {
                if (arch == CPUArch::ARMv5TE) {
                    if (bit12) {
                        Translate(arm_decoder::Undefined(), emitter);
                    } else {
                        Translate(arm_decoder::HalfwordAndSignedTransfer(opcode), emitter);
                    }
                } else {
                    Translate(arm_decoder::Undefined(), emitter);
                }
            } else if (s) {
                if (arch == CPUArch::ARMv5TE) {
                    if (bit12) {
                        Translate(arm_decoder::Undefined(), emitter);
                    } else {
                        Translate(arm_decoder::HalfwordAndSignedTransfer(opcode), emitter);
                    }
                } else {
                    Translate(arm_decoder::Undefined(), emitter);
                }
            } else if (h) {
                Translate(arm_decoder::HalfwordAndSignedTransfer(opcode), emitter);
            }
        } else if ((bits24to20 & 0b1'1011) == 0b1'0000 && (bits7to4 & 0b1111) == 0b0000) {
            Translate(arm_decoder::PSRRead(opcode), emitter);
        } else if ((bits24to20 & 0b1'1011) == 0b1'0010 && (bits7to4 & 0b1111) == 0b0000) {
            Translate(arm_decoder::PSRWrite(opcode), emitter);
        } else {
            Translate(arm_decoder::DataProcessing(opcode), emitter);
        }
        break;
    case 0b001:
        if (extendedARMv5TEInstr) {
            Translate(arm_decoder::Undefined(), emitter);
        } else if ((bits24to20 & 0b1'1011) == 0b1'0010) {
            Translate(arm_decoder::PSRWrite(opcode), emitter);
        } else if ((bits24to20 & 0b1'1011) == 0b1'0000) {
            Translate(arm_decoder::Undefined(), emitter);
        } else {
            Translate(arm_decoder::DataProcessing(opcode), emitter);
        }
        break;
    case 0b010:
    case 0b011:
        if (extendedARMv5TEInstr) {
            if ((bits24to20 & 0b1'0111) == 0b1'0101) {
                Translate(arm_decoder::Preload(opcode), emitter);
            } else {
                Translate(arm_decoder::Undefined(), emitter);
            }
        } else if (bit::test<0>(op) && bit::test<0>(bits7to4)) {
            Translate(arm_decoder::Undefined(), emitter);
        } else {
            Translate(arm_decoder::SingleDataTransfer(opcode), emitter);
        }
        break;
    case 0b100:
        if (extendedARMv5TEInstr) {
            Translate(arm_decoder::Undefined(), emitter);
        } else {
            Translate(arm_decoder::BlockTransfer(opcode), emitter);
        }
        break;
    case 0b101: {
        const bool switchToThumb = (arch == CPUArch::ARMv5TE) && (cond == Condition::NV);
        Translate(arm_decoder::BranchOffset(opcode, switchToThumb), emitter);
        break;
    }
    case 0b110:
        if (extendedARMv5TEInstr) {
            Translate(arm_decoder::CopDataTransfer(opcode, true), emitter);
        } else if (arch == CPUArch::ARMv5TE && (bits24to20 & 0b1'1110) == 0b0'0100) {
            Translate(arm_decoder::CopDualRegTransfer(opcode), emitter);
        } else {
            Translate(arm_decoder::CopDataTransfer(opcode, false), emitter);
        }
        break;
    case 0b111: {
        if (extendedARMv5TEInstr && !bit::test<24>(opcode)) {
            if (bit::test<4>(opcode)) {
                Translate(arm_decoder::CopRegTransfer(opcode, true), emitter);
            } else {
                Translate(arm_decoder::CopDataOperations(opcode, true), emitter);
            }
        } else if (extendedARMv5TEInstr && bit::test<8>(opcode)) {
            Translate(arm_decoder::Undefined(), emitter);
        } else if (bit::test<24>(opcode)) {
            Translate(arm_decoder::SoftwareInterrupt(opcode), emitter);
        } else if (bit::test<4>(opcode)) {
            Translate(arm_decoder::CopRegTransfer(opcode, false), emitter);
        } else {
            Translate(arm_decoder::CopDataOperations(opcode, false), emitter);
        }
        break;
    }
    }
}

void Translator::TranslateThumb(uint16_t opcode, Emitter &emitter) {
    const CPUArch arch = m_context.GetCPUArch();

    const uint8_t group = bit::extract<12, 4>(opcode);
    switch (group) {
    case 0b0000: [[fallthrough]];
    case 0b0001:
        if (bit::extract<11, 2>(opcode) == 0b11) {
            Translate(thumb_decoder::AddSubRegImm(opcode), emitter);
        } else {
            Translate(thumb_decoder::ShiftByImm(opcode), emitter);
        }
        break;
    case 0b0010: [[fallthrough]];
    case 0b0011: Translate(thumb_decoder::MovCmpAddSubImm(opcode), emitter); break;
    case 0b0100:
        switch (bit::extract<10, 2>(opcode)) {
        case 0b00: {
            using DPOpcode = instrs::DataProcessing::Opcode;

            const auto op = bit::extract<6, 4>(opcode);
            switch (op) {
            case 0x0 /*AND*/: Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::AND), emitter); break;
            case 0x1 /*EOR*/: Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::EOR), emitter); break;
            case 0x2 /*LSL*/: Translate(thumb_decoder::DataProcessingShift(opcode, ShiftType::LSL), emitter); break;
            case 0x3 /*LSR*/: Translate(thumb_decoder::DataProcessingShift(opcode, ShiftType::LSR), emitter); break;
            case 0x4 /*ASR*/: Translate(thumb_decoder::DataProcessingShift(opcode, ShiftType::ASR), emitter); break;
            case 0x5 /*ADC*/: Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::ADC), emitter); break;
            case 0x6 /*SBC*/: Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::SBC), emitter); break;
            case 0x7 /*ROR*/: Translate(thumb_decoder::DataProcessingShift(opcode, ShiftType::ROR), emitter); break;
            case 0x8 /*TST*/: Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::TST), emitter); break;
            case 0x9 /*NEG*/: Translate(thumb_decoder::DataProcessingNegate(opcode), emitter); break;
            case 0xA /*CMP*/: Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::CMP), emitter); break;
            case 0xB /*CMN*/: Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::CMN), emitter); break;
            case 0xC /*ORR*/: Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::ORR), emitter); break;
            case 0xD /*MUL*/: Translate(thumb_decoder::DataProcessingMultiply(opcode), emitter); break;
            case 0xE /*BIC*/: Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::BIC), emitter); break;
            case 0xF /*MVN*/: Translate(thumb_decoder::DataProcessingStandard(opcode, DPOpcode::MVN), emitter); break;
            }
            break;
        }
        case 0b01:
            if (bit::extract<8, 2>(opcode) == 0b11) {
                const bool link = (arch == CPUArch::ARMv5TE) && bit::test<7>(opcode);
                Translate(thumb_decoder::HiRegBranchExchange(opcode, link), emitter);
            } else {
                Translate(thumb_decoder::HiRegOps(opcode), emitter);
            }
            break;
        default: Translate(thumb_decoder::PCRelativeLoad(opcode), emitter); break;
        }
        break;
    case 0b0101:
        if (bit::test<9>(opcode)) {
            Translate(thumb_decoder::LoadStoreHalfRegOffset(opcode), emitter);
        } else {
            Translate(thumb_decoder::LoadStoreByteWordRegOffset(opcode), emitter);
        }
        break;
    case 0b0110: [[fallthrough]];
    case 0b0111: Translate(thumb_decoder::LoadStoreByteWordImmOffset(opcode), emitter); break;
    case 0b1000: Translate(thumb_decoder::LoadStoreHalfImmOffset(opcode), emitter); break;
    case 0b1001: Translate(thumb_decoder::SPRelativeLoadStore(opcode), emitter); break;
    case 0b1010: Translate(thumb_decoder::AddToSPOrPC(opcode), emitter); break;
    case 0b1011:
        switch (bit::extract<8, 4>(opcode)) {
        case 0b0000: Translate(thumb_decoder::AdjustSP(opcode), emitter); break;
        case 0b1110:
            if (arch == CPUArch::ARMv5TE) {
                Translate(thumb_decoder::SoftwareBreakpoint(), emitter);
            } else {
                Translate(thumb_decoder::Undefined(), emitter);
            }
            break;
        case 0b0100: [[fallthrough]];
        case 0b0101: [[fallthrough]];
        case 0b1100: [[fallthrough]];
        case 0b1101: Translate(thumb_decoder::PushPop(opcode), emitter); break;
        default: Translate(thumb_decoder::Undefined(), emitter); break;
        }
        break;
    case 0b1100: Translate(thumb_decoder::LoadStoreMultiple(opcode), emitter); break;
    case 0b1101:
        switch (bit::extract<8, 4>(opcode)) {
        case 0b1110: Translate(thumb_decoder::Undefined(), emitter); break;
        case 0b1111: Translate(thumb_decoder::SoftwareInterrupt(opcode), emitter); break;
        default: Translate(thumb_decoder::ConditionalBranch(opcode), emitter); break;
        }
        break;
    case 0b1110:
        if (arch == CPUArch::ARMv5TE) {
            const bool blx = bit::test<11>(opcode);
            if (blx) {
                if (bit::test<0>(opcode)) {
                    Translate(thumb_decoder::Undefined(), emitter);
                } else {
                    Translate(thumb_decoder::LongBranchSuffix(opcode, true), emitter);
                }
            }
        } else {
            Translate(thumb_decoder::UnconditionalBranch(opcode), emitter);
        }
        break;
    case 0b1111:
        if (bit::test<11>(opcode)) {
            Translate(thumb_decoder::LongBranchSuffix(opcode, false), emitter);
        } else {
            Translate(thumb_decoder::LongBranchPrefix(opcode), emitter);
        }
        break;
    }
}

void Translator::Translate(const BranchOffset &instr, Emitter &emitter) {
    if (instr.IsLink()) {
        emitter.LinkBeforeBranch();
    }

    const uint32_t targetAddress = emitter.CurrentPC() + instr.offset;
    if (instr.IsExchange()) {
        emitter.BranchExchange(targetAddress);
    } else {
        emitter.Branch(targetAddress);
    }

    // TODO: set block branch target
    m_endBlock = true;
}

void Translator::Translate(const BranchExchangeRegister &instr, Emitter &emitter) {
    auto addr = emitter.GetRegister(instr.reg);
    if (instr.link) {
        emitter.LinkBeforeBranch();
    }
    emitter.BranchExchange(addr);

    // TODO: set block branch target
    m_endBlock = true;
}

void Translator::Translate(const ThumbLongBranchSuffix &instr, Emitter &emitter) {
    auto lr = emitter.GetRegister(GPR::LR);
    auto targetAddrBase = emitter.Add(lr, instr.offset, false);

    emitter.LinkBeforeBranch();
    if (instr.blx) {
        emitter.BranchExchange(targetAddrBase);
    } else {
        emitter.Branch(targetAddrBase);
    }

    // TODO: set block branch target
    m_endBlock = true;
}

void Translator::Translate(const DataProcessing &instr, Emitter &emitter) {
    using Opcode = DataProcessing::Opcode;

    Variable lhs;
    if (instr.opcode != Opcode::MOV && instr.opcode != Opcode::MVN) {
        lhs = emitter.GetRegister(instr.lhsReg);
    }

    VarOrImmArg rhs;
    if (instr.immediate) {
        rhs = instr.rhs.imm;
    } else {
        rhs = emitter.BarrelShifter(instr.rhs.shift, instr.setFlags);
    }

    // When the S flag is set with Rd = 15, copy SPSR to CPSR
    if (instr.setFlags && instr.dstReg == GPR::PC) {
        emitter.CopySPSRToCPSR();
        m_flagsUpdated = true;
    }

    // Perform the selected ALU operation
    Variable result;
    switch (instr.opcode) {
    case Opcode::AND: result = emitter.BitwiseAnd(lhs, rhs, instr.setFlags); break;
    case Opcode::EOR: result = emitter.BitwiseXor(lhs, rhs, instr.setFlags); break;
    case Opcode::SUB: result = emitter.Subtract(lhs, rhs, instr.setFlags); break;
    case Opcode::RSB: result = emitter.Subtract(rhs, lhs, instr.setFlags); break; // note: swapped rhs/lhs
    case Opcode::ADD: result = emitter.Add(lhs, rhs, instr.setFlags); break;
    case Opcode::ADC: result = emitter.AddCarry(lhs, rhs, instr.setFlags); break;
    case Opcode::SBC: result = emitter.SubtractCarry(lhs, rhs, instr.setFlags); break;
    case Opcode::RSC: result = emitter.SubtractCarry(rhs, lhs, instr.setFlags); break; // note: swapped rhs/lhs
    case Opcode::TST: emitter.Test(lhs, rhs); break;
    case Opcode::TEQ: emitter.TestEquivalence(lhs, rhs); break;
    case Opcode::CMP: emitter.Compare(lhs, rhs); break;
    case Opcode::CMN: emitter.CompareNegated(lhs, rhs); break;
    case Opcode::ORR: result = emitter.BitwiseOr(lhs, rhs, instr.setFlags); break;
    case Opcode::MOV: result = emitter.Move(rhs, instr.setFlags); break;
    case Opcode::BIC: result = emitter.BitClear(lhs, rhs, instr.setFlags); break;
    case Opcode::MVN: result = emitter.MoveNegated(rhs, instr.setFlags); break;
    }

    // Store result (except for comparison operators)
    if (result.IsPresent()) {
        emitter.SetRegister(instr.dstReg, result);
    }

    // Update flags if requested (unless Rd = 15)
    if (instr.setFlags && instr.dstReg != GPR::PC) {
        static constexpr auto flagsNZ = Flags::N | Flags::Z;
        static constexpr auto flagsNZCV = flagsNZ | Flags::C | Flags::V;
        static constexpr Flags flagsByOpcode[] = {
            /*AND*/ flagsNZ,   /*EOR*/ flagsNZ,   /*SUB*/ flagsNZCV, /*RSB*/ flagsNZCV,
            /*ADD*/ flagsNZCV, /*ADC*/ flagsNZCV, /*SBC*/ flagsNZCV, /*RSC*/ flagsNZCV,
            /*TST*/ flagsNZ,   /*TEQ*/ flagsNZ,   /*CMP*/ flagsNZCV, /*CMN*/ flagsNZCV,
            /*ORR*/ flagsNZ,   /*MOV*/ flagsNZ,   /*BIC*/ flagsNZ,   /*MVN*/ flagsNZ};

        emitter.UpdateFlags(flagsByOpcode[static_cast<size_t>(instr.opcode)]);
    }

    // Branch or fetch next instruction
    if (instr.dstReg == GPR::PC) {
        // TODO: reload pipeline
        if (instr.setFlags) {
            // May also switch to thumb depending on CPSR.T
        } else {
            // Branch without switching modes; CPSR was not changed
        }
        m_endBlock = true;
    } else {
        m_flagsUpdated = instr.setFlags;

        emitter.FetchInstruction();
    }
}

void Translator::Translate(const CountLeadingZeros &instr, Emitter &emitter) {
    auto arg = emitter.GetRegister(instr.argReg);
    auto result = emitter.CountLeadingZeros(arg);
    emitter.SetRegisterExceptPC(instr.dstReg, result);

    emitter.FetchInstruction();
}

void Translator::Translate(const SaturatingAddSub &instr, Emitter &emitter) {
    auto lhs = emitter.GetRegister(instr.lhsReg);
    auto rhs = emitter.GetRegister(instr.rhsReg);
    if (instr.dbl) {
        rhs = emitter.SaturatingAdd(rhs, rhs);
        emitter.UpdateStickyOverflow();
    }

    Variable result;
    if (instr.sub) {
        result = emitter.SaturatingSubtract(lhs, rhs);
    } else {
        result = emitter.SaturatingAdd(lhs, rhs);
    }
    emitter.UpdateStickyOverflow();
    emitter.SetRegisterExceptPC(instr.dstReg, result);

    emitter.FetchInstruction();
}

void Translator::Translate(const MultiplyAccumulate &instr, Emitter &emitter) {
    auto lhs = emitter.GetRegister(instr.lhsReg);
    auto rhs = emitter.GetRegister(instr.rhsReg);

    auto result = emitter.Multiply(lhs, rhs, false, instr.setFlags);
    if (instr.accumulate) {
        auto acc = emitter.GetRegister(instr.accReg);
        result = emitter.Add(result, acc, instr.setFlags);
    }
    emitter.SetRegisterExceptPC(instr.dstReg, result);

    if (instr.setFlags) {
        emitter.UpdateFlags(Flags::N | Flags::Z);
        m_flagsUpdated = true;
    }

    emitter.FetchInstruction();
}

void Translator::Translate(const MultiplyAccumulateLong &instr, Emitter &emitter) {
    auto lhs = emitter.GetRegister(instr.lhsReg);
    auto rhs = emitter.GetRegister(instr.rhsReg);

    auto result = emitter.MultiplyLong(lhs, rhs, instr.signedMul, false, instr.setFlags);
    if (instr.accumulate) {
        auto accLo = emitter.GetRegister(instr.dstAccLoReg);
        auto accHi = emitter.GetRegister(instr.dstAccHiReg);
        result = emitter.AddLong(result.lo, result.hi, accLo, accHi, instr.setFlags);
    }
    emitter.SetRegisterExceptPC(instr.dstAccLoReg, result.lo);
    emitter.SetRegisterExceptPC(instr.dstAccHiReg, result.hi);

    if (instr.setFlags) {
        emitter.UpdateFlags(Flags::N | Flags::Z);
        m_flagsUpdated = true;
    }

    emitter.FetchInstruction();
}

void Translator::Translate(const SignedMultiplyAccumulate &instr, Emitter &emitter) {
    auto selectHalf = [&](GPR gpr, bool top) {
        auto value = emitter.GetRegister(gpr);
        if (!top) {
            value = emitter.LogicalShiftLeft(value, 16, false);
        }
        return emitter.ArithmeticShiftRight(value, 16, false);
    };

    auto lhs = selectHalf(instr.lhsReg, instr.x);
    auto rhs = selectHalf(instr.rhsReg, instr.y);

    auto result = emitter.Multiply(lhs, rhs, true, false);
    if (instr.accumulate) {
        auto acc = emitter.GetRegister(instr.accReg);
        result = emitter.Add(result, acc, true);
        emitter.UpdateStickyOverflow();
    }
    emitter.SetRegisterExceptPC(instr.dstReg, result);

    emitter.FetchInstruction();
}

void Translator::Translate(const SignedMultiplyAccumulateWord &instr, Emitter &emitter) {
    auto selectHalf = [&](GPR gpr, bool top) {
        auto value = emitter.GetRegister(gpr);
        if (!top) {
            value = emitter.LogicalShiftLeft(value, 16, false);
        }
        return emitter.ArithmeticShiftRight(value, 16, false);
    };

    auto lhs = emitter.GetRegister(instr.lhsReg);
    auto rhs = selectHalf(instr.rhsReg, instr.y);

    auto mulResult = emitter.MultiplyLong(lhs, rhs, true, true, false);
    auto result = mulResult.lo;
    if (instr.accumulate) {
        auto acc = emitter.GetRegister(instr.accReg);
        result = emitter.Add(result, acc, true);
        emitter.UpdateStickyOverflow();
    }
    emitter.SetRegisterExceptPC(instr.dstReg, result);

    emitter.FetchInstruction();
}

void Translator::Translate(const SignedMultiplyAccumulateLong &instr, Emitter &emitter) {
    auto selectHalf = [&](GPR gpr, bool top) {
        auto value = emitter.GetRegister(gpr);
        if (!top) {
            value = emitter.LogicalShiftLeft(value, 16, false);
        }
        return emitter.ArithmeticShiftRight(value, 16, false);
    };

    auto lhs = selectHalf(instr.lhsReg, instr.x);
    auto rhs = selectHalf(instr.rhsReg, instr.y);
    auto accLo = emitter.GetRegister(instr.dstAccLoReg);
    auto accHi = emitter.GetRegister(instr.dstAccHiReg);

    auto result = emitter.MultiplyLong(lhs, rhs, true, false, false);
    auto accResult = emitter.AddLong(result.lo, result.hi, accLo, accHi, false);
    emitter.SetRegisterExceptPC(instr.dstAccLoReg, accResult.lo);
    emitter.SetRegisterExceptPC(instr.dstAccHiReg, accResult.hi);

    emitter.FetchInstruction();
}

void Translator::Translate(const PSRRead &instr, Emitter &emitter) {
    Variable psr;
    if (instr.spsr) {
        psr = emitter.GetSPSR();
    } else {
        psr = emitter.GetCPSR();
    }
    emitter.SetRegisterExceptPC(instr.dstReg, psr);

    emitter.FetchInstruction();
}

void Translator::Translate(const PSRWrite &instr, Emitter &emitter) {
    uint32_t mask = 0;
    if (instr.f) {
        mask |= 0xFF000000; // (f)
    }
    if (instr.s) {
        mask |= 0x00FF0000; // (s)
    }
    if (instr.x) {
        mask |= 0x0000FF00; // (x)
    }
    if (instr.c) {
        mask |= 0x000000FF; // (c)
    }

    Variable psr;
    if (mask == 0xFFFFFFFF) {
        if (instr.immediate) {
            psr = emitter.Constant(instr.value.imm);
        } else {
            psr = emitter.GetRegister(instr.value.reg);
        }
    } else {
        if (instr.spsr) {
            psr = emitter.GetSPSR();
        } else {
            psr = emitter.GetCPSR();
        }
        psr = emitter.BitClear(psr, mask, false);

        if (instr.immediate) {
            auto value = instr.value.imm & mask;
            psr = emitter.BitwiseOr(psr, value, false);
        } else {
            auto value = emitter.GetRegister(instr.value.reg);
            value = emitter.BitwiseAnd(value, mask, false);
            psr = emitter.BitwiseOr(psr, value, false);
        }
    }

    if (instr.spsr) {
        emitter.SetSPSR(psr);
    } else {
        emitter.SetCPSR(psr);
    }

    emitter.FetchInstruction();

    if (instr.f || instr.c) {
        m_flagsUpdated = true;
    }
}

void Translator::Translate(const SingleDataTransfer &instr, Emitter &emitter) {
    Variable address;
    if (instr.preindexed) {
        address = emitter.ComputeAddress(instr.address);
    } else {
        address = emitter.GetRegister(instr.address.baseReg);
    }

    // When the W bit is set in a post-indexed operation, the transfer affects user mode registers
    GPRArg gpr = instr.reg;
    gpr.userMode = (instr.writeback && !instr.preindexed);
    const bool isPC = (gpr.gpr == GPR::PC);
    Variable pcValue{};

    Variable value{};
    if (instr.load) {
        if (instr.byte) {
            value = emitter.MemRead(MemAccessMode::Raw, MemAccessSize::Byte, address);
        } else if (isPC) {
            value = emitter.MemRead(MemAccessMode::Raw, MemAccessSize::Word, address);
        } else {
            value = emitter.MemRead(MemAccessMode::Unaligned, MemAccessSize::Word, address);
        }

        if (isPC) {
            pcValue = value;
        } else {
            emitter.SetRegister(gpr, value);
        }
    } else {
        if (isPC) {
            value = emitter.Constant(emitter.CurrentPC() + emitter.InstructionSize());
        } else {
            value = emitter.GetRegister(gpr);
        }

        if (instr.byte) {
            emitter.MemWrite(MemAccessSize::Byte, value, address);
        } else {
            emitter.MemWrite(MemAccessSize::Word, value, address);
        }
    }

    // Write back address if requested
    if (!instr.load || instr.reg != instr.address.baseReg) {
        if (!instr.preindexed) {
            address = emitter.ApplyAddressOffset(address, instr.address);
            if (instr.address.baseReg == GPR::PC) {
                pcValue = address;
            } else {
                emitter.SetRegister(instr.address.baseReg, address);
            }
        } else if (instr.writeback) {
            if (instr.address.baseReg == GPR::PC) {
                pcValue = address;
            } else {
                emitter.SetRegister(instr.address.baseReg, address);
            }
        }
    }

    if (pcValue.IsPresent()) {
        if (m_context.GetCPUArch() == CPUArch::ARMv5TE) {
            // TODO: honor CP15 pre-ARMv5 branching feature
            // if (!m_cp15.ctl.preARMv5) {
            // Switch to THUMB mode if bit 0 is set (ARMv5 CP15 feature)
            emitter.BranchExchange(pcValue);
            // } else {
            //     emitter.Branch(pcValue);
            // }
        } else {
            emitter.Branch(pcValue);
        }
        m_endBlock = true;
    } else {
        emitter.FetchInstruction();
    }
}

void Translator::Translate(const HalfwordAndSignedTransfer &instr, Emitter &emitter) {
    Variable offset{};
    if (instr.immediate) {
        if (instr.offset.imm != 0) {
            offset = emitter.Constant(instr.offset.imm);
        }
    } else {
        offset = emitter.GetRegister(instr.offset.reg);
    }

    auto address = emitter.GetRegister(instr.baseReg);
    if (offset.IsPresent() && instr.preindexed) {
        if (instr.positiveOffset) {
            address = emitter.Add(address, offset, false);
        } else {
            address = emitter.Subtract(address, offset, false);
        }
    }

    Variable pcValue{};
    bool writeback = false;
    if (instr.load) {
        Variable value;
        if (instr.sign && instr.half) {
            // LDRSH
            value = emitter.MemRead(MemAccessMode::Signed, MemAccessSize::Half, address);
        } else if (instr.sign) {
            // LDRSB
            value = emitter.MemRead(MemAccessMode::Signed, MemAccessSize::Byte, address);
        } else if (instr.half) {
            // LDRH
            value = emitter.MemRead(MemAccessMode::Unaligned, MemAccessSize::Half, address);
        } else {
            // SWP/SWPB, not handled here
            // TODO: unreachable
        }
        if (instr.reg == GPR::PC) {
            pcValue = value;
        } else {
            emitter.SetRegister(instr.reg, value);
        }
        writeback = (instr.baseReg != instr.reg);
    } else {
        if (instr.sign && instr.half) {
            // STRD
            const GPR nextReg = static_cast<GPR>(static_cast<uint8_t>(instr.reg) + 1);
            auto value1 = emitter.GetRegister(instr.reg);
            auto value2 = emitter.GetRegister(nextReg);
            auto address2 = emitter.Add(address, 4, false);
            emitter.MemWrite(MemAccessSize::Word, value1, address);
            emitter.MemWrite(MemAccessSize::Word, value2, address2);
            writeback = true;
        } else if (instr.sign) {
            // LDRD
            const GPR nextReg = static_cast<GPR>(static_cast<uint8_t>(instr.reg) + 1);
            auto address2 = emitter.Add(address, 4, false);
            auto value1 = emitter.MemRead(MemAccessMode::Unaligned, MemAccessSize::Word, address);
            auto value2 = emitter.MemRead(MemAccessMode::Unaligned, MemAccessSize::Word, address2);
            emitter.SetRegister(instr.reg, value1);
            if (nextReg == GPR::PC) {
                pcValue = value2;
            } else {
                emitter.SetRegister(nextReg, value2);
            }
            writeback = (instr.baseReg != nextReg);
        } else if (instr.half) {
            // STRH
            Variable value;
            if (instr.reg == GPR::PC) {
                value = emitter.Constant(emitter.CurrentPC() + emitter.InstructionSize());
            } else {
                value = emitter.GetRegister(instr.reg);
            }
            emitter.MemWrite(MemAccessSize::Half, value, address);
            writeback = true;
        } else {
            // SWP/SWPB, not handled here
            // TODO: unreachable
        }
    }

    // Write back address if requested
    if (writeback) {
        if (!instr.preindexed) {
            if (offset.IsPresent()) {
                if (instr.positiveOffset) {
                    address = emitter.Add(address, offset, false);
                } else {
                    address = emitter.Subtract(address, offset, false);
                }
            }
            if (instr.baseReg == GPR::PC) {
                pcValue = address;
            } else {
                emitter.SetRegister(instr.baseReg, address);
            }
        } else if (instr.writeback) {
            if (instr.baseReg == GPR::PC) {
                pcValue = address;
            } else {
                emitter.SetRegister(instr.baseReg, address);
            }
        }
    }

    if (pcValue.IsPresent()) {
        emitter.Branch(pcValue);
        m_endBlock = true;
    } else {
        emitter.FetchInstruction();
    }
}

void Translator::Translate(const BlockTransfer &instr, Emitter &emitter) {
    // Compute total transfer size and bounds
    uint32_t firstReg;
    uint32_t lastReg;
    uint32_t size;
    if (instr.regList == 0) {
        // An empty list results in transferring nothing but incrementing the address as if we had a full list
        firstReg = 15;
        lastReg = 0;
        size = 16 * 4;
    } else {
        firstReg = std::countr_zero(instr.regList);
        lastReg = 15 - std::countl_zero(instr.regList);
        size = std::popcount(instr.regList) * 4;
    }

    const bool pcIncluded = instr.regList & (1 << 15);
    const bool userModeTransfer = instr.userModeOrPSRTransfer && (!instr.load || !pcIncluded);

    // Precompute addresses
    auto address = emitter.GetRegister(instr.baseReg);
    auto startAddress = address;
    auto finalAddress = address;
    if (instr.positiveOffset) {
        finalAddress = emitter.Add(address, size, false);
    } else {
        finalAddress = emitter.Subtract(address, size, false);
        address = finalAddress;
    }

    // Registers are loaded/stored in asceding order in memory, regardless of pre/post-indexing and direction flags.
    // We can implement a loop that transfers registers without reversing the list by reversing the indexing flag
    // when the direction flag is down (U=0), which can be achieved by comparing both for equality.
    const bool preInc = (instr.preindexed == instr.positiveOffset);

    // Execute transfer
    Variable pcValue{};
    for (uint32_t i = firstReg; i <= lastReg; i++) {
        if (~instr.regList & (1 << i)) {
            continue;
        }

        const auto gpr = static_cast<GPR>(i);

        if (preInc) {
            address = emitter.Add(address, 4, false);
        }

        // Transfer data
        if (instr.load) {
            auto value = emitter.MemRead(MemAccessMode::Raw, MemAccessSize::Word, address);
            if (gpr == GPR::PC) {
                if (instr.userModeOrPSRTransfer) {
                    emitter.CopySPSRToCPSR();
                    m_flagsUpdated = true;
                }
                pcValue = value;
            } else {
                emitter.SetRegister({gpr, userModeTransfer}, value);
            }
        } else {
            Variable value{};
            if (!instr.userModeOrPSRTransfer && gpr == instr.baseReg) {
                value = (i == firstReg) ? startAddress : finalAddress;
            } else if (gpr == GPR::PC) {
                value = emitter.Constant(emitter.CurrentPC() + emitter.InstructionSize());
            } else {
                value = emitter.GetRegister({gpr, userModeTransfer});
            }
            emitter.MemWrite(MemAccessSize::Word, value, address);
        }

        if (!preInc) {
            address = emitter.Add(address, 4, false);
        }
    }

    if (instr.writeback) {
        // STMs always writeback
        // LDMs writeback depend on the CPU architecture:
        // - ARMv4T: Rn is not in the list
        // - ARMv5TE: Rn is not the last in the list, or if it's the only register in the list
        bool writeback = !instr.load;
        if (!writeback) {
            auto rn = static_cast<uint32_t>(instr.baseReg);
            switch (m_context.GetCPUArch()) {
            case CPUArch::ARMv4T: writeback = (~instr.regList & (1 << rn)); break;
            case CPUArch::ARMv5TE: writeback = (lastReg != rn || instr.regList == (1 << rn)); break;
            default: /* TODO: unreachable */ break;
            }
        }
        if (writeback) {
            if (instr.baseReg == GPR::PC) {
                pcValue = finalAddress;
            } else {
                emitter.SetRegister(instr.baseReg, finalAddress);
            }
        }
    }

    if (pcValue.IsPresent()) {
        if (m_context.GetCPUArch() == CPUArch::ARMv5TE) {
            // TODO: honor CP15 pre-ARMv5 branching feature
            // if (!m_cp15.ctl.preARMv5) {
            // Switch to THUMB mode if bit 0 is set (ARMv5 CP15 feature)
            emitter.BranchExchange(pcValue);
            // } else {
            //     emitter.Branch(pcValue);
            // }
        } else {
            emitter.Branch(pcValue);
        }
        m_endBlock = true;
    } else {
        emitter.FetchInstruction();
    }
}

void Translator::Translate(const SingleDataSwap &instr, Emitter &emitter) {
    auto address = emitter.GetRegister(instr.addressReg);
    auto src = emitter.GetRegister(instr.valueReg);

    // Perform the swap
    Variable value{};
    if (instr.byte) {
        value = emitter.MemRead(MemAccessMode::Raw, MemAccessSize::Byte, address);
        emitter.MemWrite(MemAccessSize::Byte, src, address);
    } else {
        value = emitter.MemRead(MemAccessMode::Unaligned, MemAccessSize::Word, address);
        emitter.MemWrite(MemAccessSize::Word, src, address);
    }
    emitter.SetRegisterExceptPC(instr.dstReg, value);

    emitter.FetchInstruction();
}

void Translator::Translate(const SoftwareInterrupt &instr, Emitter &emitter) {
    emitter.EnterException(arm::Exception::SoftwareInterrupt);
    m_endBlock = true;
}

void Translator::Translate(const SoftwareBreakpoint &instr, Emitter &emitter) {
    emitter.EnterException(arm::Exception::PrefetchAbort);
    m_endBlock = true;
}

void Translator::Translate(const Preload &instr, Emitter &emitter) {
    auto address = emitter.ComputeAddress(instr.address);
    emitter.Preload(address);

    emitter.FetchInstruction();
}

void Translator::Translate(const CopDataOperations &instr, Emitter &emitter) {
    // TODO: implement
    emitter.EnterException(arm::Exception::UndefinedInstruction);
    m_endBlock = true;
}

void Translator::Translate(const CopDataTransfer &instr, Emitter &emitter) {
    // TODO: implement
    emitter.EnterException(arm::Exception::UndefinedInstruction);
    m_endBlock = true;
}

void Translator::Translate(const CopRegTransfer &instr, Emitter &emitter) {
    auto &cop = m_context.GetCoprocessor(instr.cpnum);
    if (!cop.IsPresent()) {
        emitter.EnterException(arm::Exception::UndefinedInstruction);
        m_endBlock = true;
        return;
    }
    if (instr.ext && !cop.SupportsExtendedRegTransfers()) {
        emitter.EnterException(arm::Exception::UndefinedInstruction);
        m_endBlock = true;
        return;
    }

    if (instr.store) {
        auto value = emitter.GetRegister(instr.rd);
        emitter.StoreCopRegister(instr.cpnum, instr.opcode1, instr.crn, instr.crm, instr.opcode2, instr.ext, value);
        if (cop.RegStoreHasSideEffects(instr.opcode1, instr.crn, instr.crm, instr.opcode2)) {
            m_endBlock = true;
        }
    } else {
        auto value =
            emitter.LoadCopRegister(instr.cpnum, instr.opcode1, instr.crn, instr.crm, instr.opcode2, instr.ext);

        if (instr.rd == GPR::PC) {
            // Update NZCV flags instead
            auto cpsr = emitter.GetCPSR();
            cpsr = emitter.BitClear(cpsr, 0xF0000000, false);
            value = emitter.BitwiseAnd(value, 0xF0000000, false);
            value = emitter.BitwiseOr(value, cpsr, false);
            emitter.SetCPSR(value);
            m_flagsUpdated = true;
        } else {
            emitter.SetRegister(instr.rd, value);
        }
    }

    emitter.FetchInstruction();
}

void Translator::Translate(const CopDualRegTransfer &instr, Emitter &emitter) {
    // TODO: implement
    emitter.EnterException(arm::Exception::UndefinedInstruction);
    m_endBlock = true;
}

void Translator::Translate(const Undefined &instr, Emitter &emitter) {
    emitter.EnterException(arm::Exception::UndefinedInstruction);
    m_endBlock = true;
}

} // namespace armajitto::ir
