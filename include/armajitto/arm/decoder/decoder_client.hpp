#pragma once

#include "decoder_instrs.hpp"

#include <concepts>

namespace armajitto {

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
    { client.Process(arm::instrs::Branch{}) } -> std::same_as<DecoderAction>;
    { client.Process(arm::instrs::BranchAndExchange{}) } -> std::same_as<DecoderAction>;
    { client.Process(arm::instrs::ThumbLongBranchSuffix{}) } -> std::same_as<DecoderAction>;
    { client.Process(arm::instrs::DataProcessing{}) } -> std::same_as<DecoderAction>;
    { client.Process(arm::instrs::CountLeadingZeros{}) } -> std::same_as<DecoderAction>;
    { client.Process(arm::instrs::SaturatingAddSub{}) } -> std::same_as<DecoderAction>;
    { client.Process(arm::instrs::MultiplyAccumulate{}) } -> std::same_as<DecoderAction>;
    { client.Process(arm::instrs::MultiplyAccumulateLong{}) } -> std::same_as<DecoderAction>;
    { client.Process(arm::instrs::SignedMultiplyAccumulate{}) } -> std::same_as<DecoderAction>;
    { client.Process(arm::instrs::SignedMultiplyAccumulateWord{}) } -> std::same_as<DecoderAction>;
    { client.Process(arm::instrs::SignedMultiplyAccumulateLong{}) } -> std::same_as<DecoderAction>;
    { client.Process(arm::instrs::PSRRead{}) } -> std::same_as<DecoderAction>;
    { client.Process(arm::instrs::PSRWrite{}) } -> std::same_as<DecoderAction>;
    { client.Process(arm::instrs::SingleDataTransfer{}) } -> std::same_as<DecoderAction>;
    { client.Process(arm::instrs::HalfwordAndSignedTransfer{}) } -> std::same_as<DecoderAction>;
    { client.Process(arm::instrs::BlockTransfer{}) } -> std::same_as<DecoderAction>;
    { client.Process(arm::instrs::SingleDataSwap{}) } -> std::same_as<DecoderAction>;
    { client.Process(arm::instrs::SoftwareInterrupt{}) } -> std::same_as<DecoderAction>;
    { client.Process(arm::instrs::SoftwareBreakpoint{}) } -> std::same_as<DecoderAction>;
    { client.Process(arm::instrs::Preload{}) } -> std::same_as<DecoderAction>;
    { client.Process(arm::instrs::CopDataOperations{}) } -> std::same_as<DecoderAction>;
    { client.Process(arm::instrs::CopDataTransfer{}) } -> std::same_as<DecoderAction>;
    { client.Process(arm::instrs::CopRegTransfer{}) } -> std::same_as<DecoderAction>;
    { client.Process(arm::instrs::CopDualRegTransfer{}) } -> std::same_as<DecoderAction>;
    { client.Process(arm::instrs::Undefined{}) } -> std::same_as<DecoderAction>;
};

} // namespace armajitto
