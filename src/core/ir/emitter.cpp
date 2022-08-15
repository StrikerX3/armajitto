#include "armajitto/core/ir/emitter.hpp"

using namespace armajitto::arm;
using namespace armajitto::arm::instrs;

namespace armajitto::ir {

CPUArch Emitter::GetCPUArch() {
    // TODO: implement
    return CPUArch::ARMv5TE;
}

uint16_t Emitter::CodeReadHalf(uint32_t address) {
    // TODO: implement
    return 0;
}

uint32_t Emitter::CodeReadWord(uint32_t address) {
    // TODO: implement
    return 0;
}

DecoderAction Emitter::Process(const Branch &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

DecoderAction Emitter::Process(const BranchAndExchange &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

DecoderAction Emitter::Process(const ThumbLongBranchSuffix &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

DecoderAction Emitter::Process(const DataProcessing &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

DecoderAction Emitter::Process(const CountLeadingZeros &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

DecoderAction Emitter::Process(const SaturatingAddSub &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

DecoderAction Emitter::Process(const MultiplyAccumulate &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

DecoderAction Emitter::Process(const MultiplyAccumulateLong &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

DecoderAction Emitter::Process(const SignedMultiplyAccumulate &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

DecoderAction Emitter::Process(const SignedMultiplyAccumulateWord &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

DecoderAction Emitter::Process(const SignedMultiplyAccumulateLong &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

DecoderAction Emitter::Process(const PSRRead &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

DecoderAction Emitter::Process(const PSRWrite &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

DecoderAction Emitter::Process(const SingleDataTransfer &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

DecoderAction Emitter::Process(const HalfwordAndSignedTransfer &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

DecoderAction Emitter::Process(const BlockTransfer &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

DecoderAction Emitter::Process(const SingleDataSwap &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

DecoderAction Emitter::Process(const SoftwareInterrupt &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

DecoderAction Emitter::Process(const SoftwareBreakpoint &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

DecoderAction Emitter::Process(const Preload &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

DecoderAction Emitter::Process(const CopDataOperations &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

DecoderAction Emitter::Process(const CopDataTransfer &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

DecoderAction Emitter::Process(const CopRegTransfer &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

DecoderAction Emitter::Process(const CopDualRegTransfer &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

DecoderAction Emitter::Process(const Undefined &instr) {
    // TODO: implement
    return DecoderAction::Continue;
}

} // namespace armajitto::ir