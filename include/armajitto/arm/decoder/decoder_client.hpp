#pragma once

#include "decoder_instrs.hpp"

#include <concepts>

namespace armajitto::arm {

enum class DecoderAction {
    Continue, // Decode next instruction in the current block
    Split,    // Create a new micro block and continue decoding
    End,      // Finish basic block and stop decoding

    UnmappedInstruction, // Decoder failed to decode an instruction
    Unimplemented,       // Decoder reached an unimplemented portion of code
};

template <typename T>
concept CodeAccessor = requires(T mem) {
    { mem.CodeReadHalf(uint32_t{}) } -> std::same_as<uint16_t>;
    { mem.CodeReadWord(uint32_t{}) } -> std::same_as<uint32_t>;
};

template <typename T>
concept DecoderClient = requires(T client) {
    { client.Process(instrs::Branch{}) } -> std::same_as<DecoderAction>;
    { client.Process(instrs::BranchAndExchange{}) } -> std::same_as<DecoderAction>;
    { client.Process(instrs::ThumbLongBranchSuffix{}) } -> std::same_as<DecoderAction>;
    { client.Process(instrs::DataProcessing{}) } -> std::same_as<DecoderAction>;
    { client.Process(instrs::CountLeadingZeros{}) } -> std::same_as<DecoderAction>;
    { client.Process(instrs::SaturatingAddSub{}) } -> std::same_as<DecoderAction>;
    { client.Process(instrs::MultiplyAccumulate{}) } -> std::same_as<DecoderAction>;
    { client.Process(instrs::MultiplyAccumulateLong{}) } -> std::same_as<DecoderAction>;
    { client.Process(instrs::SignedMultiplyAccumulate{}) } -> std::same_as<DecoderAction>;
    { client.Process(instrs::SignedMultiplyAccumulateWord{}) } -> std::same_as<DecoderAction>;
    { client.Process(instrs::SignedMultiplyAccumulateLong{}) } -> std::same_as<DecoderAction>;
    { client.Process(instrs::PSRRead{}) } -> std::same_as<DecoderAction>;
    { client.Process(instrs::PSRWrite{}) } -> std::same_as<DecoderAction>;
    { client.Process(instrs::SingleDataTransfer{}) } -> std::same_as<DecoderAction>;
    { client.Process(instrs::HalfwordAndSignedTransfer{}) } -> std::same_as<DecoderAction>;
    { client.Process(instrs::BlockTransfer{}) } -> std::same_as<DecoderAction>;
    { client.Process(instrs::SingleDataSwap{}) } -> std::same_as<DecoderAction>;
    { client.Process(instrs::SoftwareInterrupt{}) } -> std::same_as<DecoderAction>;
    { client.Process(instrs::SoftwareBreakpoint{}) } -> std::same_as<DecoderAction>;
    { client.Process(instrs::Preload{}) } -> std::same_as<DecoderAction>;
    { client.Process(instrs::CopDataOperations{}) } -> std::same_as<DecoderAction>;
    { client.Process(instrs::CopDataTransfer{}) } -> std::same_as<DecoderAction>;
    { client.Process(instrs::CopRegTransfer{}) } -> std::same_as<DecoderAction>;
    { client.Process(instrs::CopDualRegTransfer{}) } -> std::same_as<DecoderAction>;
    { client.Process(instrs::Undefined{}) } -> std::same_as<DecoderAction>;
};

} // namespace armajitto::arm
