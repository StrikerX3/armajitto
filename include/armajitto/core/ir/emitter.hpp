#pragma once

#include "armajitto/arm/decoder.hpp"
#include "armajitto/defs/cpu_arch.hpp"

#include <cstdint>
#include <vector>

namespace armajitto::ir {

class Emitter {
public:
    // --- Decoder client implementation ---------------------------------------

    DecoderAction Process(const arm::instrs::Branch &instr);
    DecoderAction Process(const arm::instrs::BranchAndExchange &instr);
    DecoderAction Process(const arm::instrs::ThumbLongBranchSuffix &instr);
    DecoderAction Process(const arm::instrs::DataProcessing &instr);
    DecoderAction Process(const arm::instrs::CountLeadingZeros &instr);
    DecoderAction Process(const arm::instrs::SaturatingAddSub &instr);
    DecoderAction Process(const arm::instrs::MultiplyAccumulate &instr);
    DecoderAction Process(const arm::instrs::MultiplyAccumulateLong &instr);
    DecoderAction Process(const arm::instrs::SignedMultiplyAccumulate &instr);
    DecoderAction Process(const arm::instrs::SignedMultiplyAccumulateWord &instr);
    DecoderAction Process(const arm::instrs::SignedMultiplyAccumulateLong &instr);
    DecoderAction Process(const arm::instrs::PSRRead &instr);
    DecoderAction Process(const arm::instrs::PSRWrite &instr);
    DecoderAction Process(const arm::instrs::SingleDataTransfer &instr);
    DecoderAction Process(const arm::instrs::HalfwordAndSignedTransfer &instr);
    DecoderAction Process(const arm::instrs::BlockTransfer &instr);
    DecoderAction Process(const arm::instrs::SingleDataSwap &instr);
    DecoderAction Process(const arm::instrs::SoftwareInterrupt &instr);
    DecoderAction Process(const arm::instrs::SoftwareBreakpoint &instr);
    DecoderAction Process(const arm::instrs::Preload &instr);
    DecoderAction Process(const arm::instrs::CopDataOperations &instr);
    DecoderAction Process(const arm::instrs::CopDataTransfer &instr);
    DecoderAction Process(const arm::instrs::CopRegTransfer &instr);
    DecoderAction Process(const arm::instrs::CopDualRegTransfer &instr);
    DecoderAction Process(const arm::instrs::Undefined &instr);

private:
    // TODO: reference to basic block
};

} // namespace armajitto::ir
