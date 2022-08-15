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

    Action DecodeARM(uint32_t opcode, IRCodeFragment &codeFrag);
    Action DecodeThumb(uint16_t opcode, IRCodeFragment &codeFrag);

    Action Translate(const arm::instrs::Branch &instr, IRCodeFragment &codeFrag);
    Action Translate(const arm::instrs::BranchAndExchange &instr, IRCodeFragment &codeFrag);
    Action Translate(const arm::instrs::ThumbLongBranchSuffix &instr, IRCodeFragment &codeFrag);
    Action Translate(const arm::instrs::DataProcessing &instr, IRCodeFragment &codeFrag);
    Action Translate(const arm::instrs::CountLeadingZeros &instr, IRCodeFragment &codeFrag);
    Action Translate(const arm::instrs::SaturatingAddSub &instr, IRCodeFragment &codeFrag);
    Action Translate(const arm::instrs::MultiplyAccumulate &instr, IRCodeFragment &codeFrag);
    Action Translate(const arm::instrs::MultiplyAccumulateLong &instr, IRCodeFragment &codeFrag);
    Action Translate(const arm::instrs::SignedMultiplyAccumulate &instr, IRCodeFragment &codeFrag);
    Action Translate(const arm::instrs::SignedMultiplyAccumulateWord &instr, IRCodeFragment &codeFrag);
    Action Translate(const arm::instrs::SignedMultiplyAccumulateLong &instr, IRCodeFragment &codeFrag);
    Action Translate(const arm::instrs::PSRRead &instr, IRCodeFragment &codeFrag);
    Action Translate(const arm::instrs::PSRWrite &instr, IRCodeFragment &codeFrag);
    Action Translate(const arm::instrs::SingleDataTransfer &instr, IRCodeFragment &codeFrag);
    Action Translate(const arm::instrs::HalfwordAndSignedTransfer &instr, IRCodeFragment &codeFrag);
    Action Translate(const arm::instrs::BlockTransfer &instr, IRCodeFragment &codeFrag);
    Action Translate(const arm::instrs::SingleDataSwap &instr, IRCodeFragment &codeFrag);
    Action Translate(const arm::instrs::SoftwareInterrupt &instr, IRCodeFragment &codeFrag);
    Action Translate(const arm::instrs::SoftwareBreakpoint &instr, IRCodeFragment &codeFrag);
    Action Translate(const arm::instrs::Preload &instr, IRCodeFragment &codeFrag);
    Action Translate(const arm::instrs::CopDataOperations &instr, IRCodeFragment &codeFrag);
    Action Translate(const arm::instrs::CopDataTransfer &instr, IRCodeFragment &codeFrag);
    Action Translate(const arm::instrs::CopRegTransfer &instr, IRCodeFragment &codeFrag);
    Action Translate(const arm::instrs::CopDualRegTransfer &instr, IRCodeFragment &codeFrag);
    Action Translate(const arm::instrs::Undefined &instr, IRCodeFragment &codeFrag);
};

} // namespace armajitto::ir
