#pragma once

#include "armajitto/core/context.hpp"
#include "armajitto/defs/arm/instructions.hpp"
#include "armajitto/util/bit_ops.hpp"
#include "defs/basic_block.hpp"
#include "emitter.hpp"

namespace armajitto::ir {

class Translator {
public:
    Translator(Context &context)
        : m_context(context) {}

    void TranslateARM(uint32_t address, BasicBlock &block);
    void TranslateThumb(uint32_t address, BasicBlock &block);

private:
    Context &m_context;

    enum class Action {
        Continue, // Decode next instruction in the current block
        Split,    // Create a new micro block and continue decoding
        End,      // Finish basic block and stop decoding

        UnmappedInstruction, // Decoder failed to decode an instruction
        Unimplemented,       // Decoder reached an unimplemented portion of code
    };

    Action DecodeARM(uint32_t opcode);
    Action DecodeThumb(uint16_t opcode);

    Action Translate(const arm::instrs::Branch &instr);
    Action Translate(const arm::instrs::BranchAndExchange &instr);
    Action Translate(const arm::instrs::ThumbLongBranchSuffix &instr);
    Action Translate(const arm::instrs::DataProcessing &instr);
    Action Translate(const arm::instrs::CountLeadingZeros &instr);
    Action Translate(const arm::instrs::SaturatingAddSub &instr);
    Action Translate(const arm::instrs::MultiplyAccumulate &instr);
    Action Translate(const arm::instrs::MultiplyAccumulateLong &instr);
    Action Translate(const arm::instrs::SignedMultiplyAccumulate &instr);
    Action Translate(const arm::instrs::SignedMultiplyAccumulateWord &instr);
    Action Translate(const arm::instrs::SignedMultiplyAccumulateLong &instr);
    Action Translate(const arm::instrs::PSRRead &instr);
    Action Translate(const arm::instrs::PSRWrite &instr);
    Action Translate(const arm::instrs::SingleDataTransfer &instr);
    Action Translate(const arm::instrs::HalfwordAndSignedTransfer &instr);
    Action Translate(const arm::instrs::BlockTransfer &instr);
    Action Translate(const arm::instrs::SingleDataSwap &instr);
    Action Translate(const arm::instrs::SoftwareInterrupt &instr);
    Action Translate(const arm::instrs::SoftwareBreakpoint &instr);
    Action Translate(const arm::instrs::Preload &instr);
    Action Translate(const arm::instrs::CopDataOperations &instr);
    Action Translate(const arm::instrs::CopDataTransfer &instr);
    Action Translate(const arm::instrs::CopRegTransfer &instr);
    Action Translate(const arm::instrs::CopDualRegTransfer &instr);
    Action Translate(const arm::instrs::Undefined &instr);
};

} // namespace armajitto::ir
