#include "armajitto/core/ir/emitter.hpp"

using namespace armajitto::arm;
using namespace armajitto::arm::instrs;

namespace armajitto::ir {

DecoderAction Emitter::Process(const Branch &instr) {
    auto *op = new IRBranchOp();
    op->address.immediate = true;
    op->address.arg.imm.value = instr.offset;
    // op->dstPC;
    op->srcCPSR.immediate = false;
    // op->srcCPSR.arg.var;
    m_block.push_back(op);

    // TODO: implement
    return DecoderAction::End;
}

DecoderAction Emitter::Process(const BranchAndExchange &instr) {
    // TODO: implement
    return DecoderAction::Unimplemented;
}

DecoderAction Emitter::Process(const ThumbLongBranchSuffix &instr) {
    // TODO: implement
    return DecoderAction::Unimplemented;
}

DecoderAction Emitter::Process(const DataProcessing &instr) {
    // TODO: implement
    return DecoderAction::Unimplemented;
}

DecoderAction Emitter::Process(const CountLeadingZeros &instr) {
    // TODO: implement
    return DecoderAction::Unimplemented;
}

DecoderAction Emitter::Process(const SaturatingAddSub &instr) {
    // TODO: implement
    return DecoderAction::Unimplemented;
}

DecoderAction Emitter::Process(const MultiplyAccumulate &instr) {
    // TODO: implement
    return DecoderAction::Unimplemented;
}

DecoderAction Emitter::Process(const MultiplyAccumulateLong &instr) {
    // TODO: implement
    return DecoderAction::Unimplemented;
}

DecoderAction Emitter::Process(const SignedMultiplyAccumulate &instr) {
    // TODO: implement
    return DecoderAction::Unimplemented;
}

DecoderAction Emitter::Process(const SignedMultiplyAccumulateWord &instr) {
    // TODO: implement
    return DecoderAction::Unimplemented;
}

DecoderAction Emitter::Process(const SignedMultiplyAccumulateLong &instr) {
    // TODO: implement
    return DecoderAction::Unimplemented;
}

DecoderAction Emitter::Process(const PSRRead &instr) {
    // TODO: implement
    return DecoderAction::Unimplemented;
}

DecoderAction Emitter::Process(const PSRWrite &instr) {
    // TODO: implement
    return DecoderAction::Unimplemented;
}

DecoderAction Emitter::Process(const SingleDataTransfer &instr) {
    // TODO: implement
    return DecoderAction::Unimplemented;
}

DecoderAction Emitter::Process(const HalfwordAndSignedTransfer &instr) {
    // TODO: implement
    return DecoderAction::Unimplemented;
}

DecoderAction Emitter::Process(const BlockTransfer &instr) {
    // TODO: implement
    return DecoderAction::Unimplemented;
}

DecoderAction Emitter::Process(const SingleDataSwap &instr) {
    // TODO: implement
    return DecoderAction::Unimplemented;
}

DecoderAction Emitter::Process(const SoftwareInterrupt &instr) {
    // TODO: implement
    return DecoderAction::Unimplemented;
}

DecoderAction Emitter::Process(const SoftwareBreakpoint &instr) {
    // TODO: implement
    return DecoderAction::Unimplemented;
}

DecoderAction Emitter::Process(const Preload &instr) {
    // TODO: implement
    return DecoderAction::Unimplemented;
}

DecoderAction Emitter::Process(const CopDataOperations &instr) {
    // TODO: implement
    return DecoderAction::Unimplemented;
}

DecoderAction Emitter::Process(const CopDataTransfer &instr) {
    // TODO: implement
    return DecoderAction::Unimplemented;
}

DecoderAction Emitter::Process(const CopRegTransfer &instr) {
    // TODO: implement
    return DecoderAction::Unimplemented;
}

DecoderAction Emitter::Process(const CopDualRegTransfer &instr) {
    // TODO: implement
    return DecoderAction::Unimplemented;
}

DecoderAction Emitter::Process(const Undefined &instr) {
    // TODO: implement
    return DecoderAction::Unimplemented;
}

} // namespace armajitto::ir