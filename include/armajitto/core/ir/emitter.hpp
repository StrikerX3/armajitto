#pragma once

#include "armajitto/arm/decoder.hpp"
#include "armajitto/defs/cpu_arch.hpp"
#include "opcodes.hpp"

#include <cstdint>

namespace armajitto::ir {

class Emitter {
public:
    // --- Decoder client implementation ---------------------------------------

    CPUArch GetCPUArch();

    // Code access
    uint16_t CodeReadHalf(uint32_t address);
    uint32_t CodeReadWord(uint32_t address);

    // Instruction handlers
    arm::DecoderAction Process(const arm::instrs::Branch &instr);
    arm::DecoderAction Process(const arm::instrs::BranchAndExchange &instr);
    arm::DecoderAction Process(const arm::instrs::ThumbLongBranchSuffix &instr);
    arm::DecoderAction Process(const arm::instrs::DataProcessing &instr);
    arm::DecoderAction Process(const arm::instrs::CountLeadingZeros &instr);
    arm::DecoderAction Process(const arm::instrs::SaturatingAddSub &instr);
    arm::DecoderAction Process(const arm::instrs::MultiplyAccumulate &instr);
    arm::DecoderAction Process(const arm::instrs::MultiplyAccumulateLong &instr);
    arm::DecoderAction Process(const arm::instrs::SignedMultiplyAccumulate &instr);
    arm::DecoderAction Process(const arm::instrs::SignedMultiplyAccumulateWord &instr);
    arm::DecoderAction Process(const arm::instrs::SignedMultiplyAccumulateLong &instr);
    arm::DecoderAction Process(const arm::instrs::PSRRead &instr);
    arm::DecoderAction Process(const arm::instrs::PSRWrite &instr);
    arm::DecoderAction Process(const arm::instrs::SingleDataTransfer &instr);
    arm::DecoderAction Process(const arm::instrs::HalfwordAndSignedTransfer &instr);
    arm::DecoderAction Process(const arm::instrs::BlockTransfer &instr);
    arm::DecoderAction Process(const arm::instrs::SingleDataSwap &instr);
    arm::DecoderAction Process(const arm::instrs::SoftwareInterrupt &instr);
    arm::DecoderAction Process(const arm::instrs::SoftwareBreakpoint &instr);
    arm::DecoderAction Process(const arm::instrs::Preload &instr);
    arm::DecoderAction Process(const arm::instrs::CopDataOperations &instr);
    arm::DecoderAction Process(const arm::instrs::CopDataTransfer &instr);
    arm::DecoderAction Process(const arm::instrs::CopRegTransfer &instr);
    arm::DecoderAction Process(const arm::instrs::CopDualRegTransfer &instr);
    arm::DecoderAction Process(const arm::instrs::Undefined &instr);

private:
};

} // namespace armajitto::ir
