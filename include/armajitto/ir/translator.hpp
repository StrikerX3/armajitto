#pragma once

#include "armajitto/core/context.hpp"
#include "armajitto/defs/arm/instructions.hpp"
#include "emitter.hpp"

namespace armajitto::ir {

// Decodes and translates ARM or Thumb instructions to armajitto's intermediate representation into a basic block.
//
// A Translator instance is not safe for concurrent accesses. Use one instance per thread instead.
class Translator {
public:
    struct Parameters {
        uint32_t maxBlockSize;
    };

    Translator(Context &context, Parameters params)
        : m_context(context)
        , m_params(params) {}

    void Translate(BasicBlock &block);

private:
    Context &m_context;
    const Parameters m_params;

    bool m_flagsUpdated = false;
    bool m_endBlock = false;

    void TranslateARM(uint32_t opcode, Emitter &emitter);
    void TranslateThumb(uint16_t opcode, Emitter &emitter);

    void Translate(const arm::instrs::BranchOffset &instr, Emitter &emitter);
    void Translate(const arm::instrs::BranchExchangeRegister &instr, Emitter &emitter);
    void Translate(const arm::instrs::ThumbLongBranchSuffix &instr, Emitter &emitter);
    void Translate(const arm::instrs::DataProcessing &instr, Emitter &emitter);
    void Translate(const arm::instrs::CountLeadingZeros &instr, Emitter &emitter);
    void Translate(const arm::instrs::SaturatingAddSub &instr, Emitter &emitter);
    void Translate(const arm::instrs::MultiplyAccumulate &instr, Emitter &emitter);
    void Translate(const arm::instrs::MultiplyAccumulateLong &instr, Emitter &emitter);
    void Translate(const arm::instrs::SignedMultiplyAccumulate &instr, Emitter &emitter);
    void Translate(const arm::instrs::SignedMultiplyAccumulateWord &instr, Emitter &emitter);
    void Translate(const arm::instrs::SignedMultiplyAccumulateLong &instr, Emitter &emitter);
    void Translate(const arm::instrs::PSRRead &instr, Emitter &emitter);
    void Translate(const arm::instrs::PSRWrite &instr, Emitter &emitter);
    void Translate(const arm::instrs::SingleDataTransfer &instr, Emitter &emitter);
    void Translate(const arm::instrs::HalfwordAndSignedTransfer &instr, Emitter &emitter);
    void Translate(const arm::instrs::BlockTransfer &instr, Emitter &emitter);
    void Translate(const arm::instrs::SingleDataSwap &instr, Emitter &emitter);
    void Translate(const arm::instrs::SoftwareInterrupt &instr, Emitter &emitter);
    void Translate(const arm::instrs::SoftwareBreakpoint &instr, Emitter &emitter);
    void Translate(const arm::instrs::Preload &instr, Emitter &emitter);
    void Translate(const arm::instrs::CopDataOperations &instr, Emitter &emitter);
    void Translate(const arm::instrs::CopDataTransfer &instr, Emitter &emitter);
    void Translate(const arm::instrs::CopRegTransfer &instr, Emitter &emitter);
    void Translate(const arm::instrs::CopDualRegTransfer &instr, Emitter &emitter);
    void Translate(const arm::instrs::Undefined &instr, Emitter &emitter);
};

} // namespace armajitto::ir