#pragma once

#include "armajitto/core/context.hpp"
#include "armajitto/defs/arm/instructions.hpp"
#include "basic_block.hpp"

namespace armajitto::ir {

class Translator {
public:
    Translator(Context &context)
        : m_context(context) {}

    void TranslateARM(BasicBlock &block, uint32_t startAddress, uint32_t maxBlockSize);
    void TranslateThumb(BasicBlock &block, uint32_t startAddress, uint32_t maxBlockSize);

private:
    Context &m_context;

    template <typename DecodeFn>
    void TranslateCommon(BasicBlock &block, uint32_t startAddress, uint32_t maxBlockSize, DecodeFn &&decodeFn);

    enum class Action {
        Continue, // Decode next instruction in the current code fragment
        Split,    // Create a new code fragment and continue decoding
        End,      // Finish basic block and stop decoding

        UnmappedInstruction, // Decoder failed to decode an instruction
        Unimplemented,       // Decoder reached an unimplemented portion of code
    };

    Action DecodeARM(uint32_t opcode, Emitter &emitter);
    Action DecodeThumb(uint16_t opcode, Emitter &emitter);

    Action Translate(const arm::instrs::Branch &instr, Emitter &emitter);
    Action Translate(const arm::instrs::BranchAndExchange &instr, Emitter &emitter);
    Action Translate(const arm::instrs::ThumbLongBranchSuffix &instr, Emitter &emitter);
    Action Translate(const arm::instrs::DataProcessing &instr, Emitter &emitter);
    Action Translate(const arm::instrs::CountLeadingZeros &instr, Emitter &emitter);
    Action Translate(const arm::instrs::SaturatingAddSub &instr, Emitter &emitter);
    Action Translate(const arm::instrs::MultiplyAccumulate &instr, Emitter &emitter);
    Action Translate(const arm::instrs::MultiplyAccumulateLong &instr, Emitter &emitter);
    Action Translate(const arm::instrs::SignedMultiplyAccumulate &instr, Emitter &emitter);
    Action Translate(const arm::instrs::SignedMultiplyAccumulateWord &instr, Emitter &emitter);
    Action Translate(const arm::instrs::SignedMultiplyAccumulateLong &instr, Emitter &emitter);
    Action Translate(const arm::instrs::PSRRead &instr, Emitter &emitter);
    Action Translate(const arm::instrs::PSRWrite &instr, Emitter &emitter);
    Action Translate(const arm::instrs::SingleDataTransfer &instr, Emitter &emitter);
    Action Translate(const arm::instrs::HalfwordAndSignedTransfer &instr, Emitter &emitter);
    Action Translate(const arm::instrs::BlockTransfer &instr, Emitter &emitter);
    Action Translate(const arm::instrs::SingleDataSwap &instr, Emitter &emitter);
    Action Translate(const arm::instrs::SoftwareInterrupt &instr, Emitter &emitter);
    Action Translate(const arm::instrs::SoftwareBreakpoint &instr, Emitter &emitter);
    Action Translate(const arm::instrs::Preload &instr, Emitter &emitter);
    Action Translate(const arm::instrs::CopDataOperations &instr, Emitter &emitter);
    Action Translate(const arm::instrs::CopDataTransfer &instr, Emitter &emitter);
    Action Translate(const arm::instrs::CopRegTransfer &instr, Emitter &emitter);
    Action Translate(const arm::instrs::CopDualRegTransfer &instr, Emitter &emitter);
    Action Translate(const arm::instrs::Undefined &instr, Emitter &emitter);
};

} // namespace armajitto::ir
