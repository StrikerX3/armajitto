#pragma once

#include "armajitto/arm/decoder.hpp"
#include "armajitto/core/ir/ops/ir_ops.hpp"
#include "armajitto/defs/cpu_arch.hpp"

#include <cstdint>
#include <vector>

namespace armajitto::ir {

// TODO: this is obviously a placeholder just to get things going
using _placeholder_Block = std::vector<IROpBase *>;

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

    const _placeholder_Block &GetBlock() const {
        return m_block;
    }

private:
    // TODO: reference to basic block
    _placeholder_Block m_block;
};

} // namespace armajitto::ir
