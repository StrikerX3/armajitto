#pragma once

#include "decoder_instrs.hpp"

#include <concepts>

namespace armajitto::arm::decoder {

enum class Action {
    Continue,
    // TODO: other decoder actions

    UnmappedInstruction,
};

template <typename T>
concept Client = requires(T client) {
    { client.GetCPUArch() } -> std::same_as<CPUArch>;

    // Code access
    { client.CodeReadHalf(uint32_t{}) } -> std::same_as<uint16_t>;
    { client.CodeReadWord(uint32_t{}) } -> std::same_as<uint32_t>;

    // Instruction handlers
    { client.Process(instrs::Branch{}) } -> std::same_as<Action>;
    { client.Process(instrs::BranchAndExchange{}) } -> std::same_as<Action>;
    { client.Process(instrs::ThumbLongBranchSuffix{}) } -> std::same_as<Action>;
    { client.Process(instrs::DataProcessing{}) } -> std::same_as<Action>;
    { client.Process(instrs::CountLeadingZeros{}) } -> std::same_as<Action>;
    { client.Process(instrs::SaturatingAddSub{}) } -> std::same_as<Action>;
    { client.Process(instrs::MultiplyAccumulate{}) } -> std::same_as<Action>;
    { client.Process(instrs::MultiplyAccumulateLong{}) } -> std::same_as<Action>;
    { client.Process(instrs::SignedMultiplyAccumulate{}) } -> std::same_as<Action>;
    { client.Process(instrs::SignedMultiplyAccumulateWord{}) } -> std::same_as<Action>;
    { client.Process(instrs::SignedMultiplyAccumulateLong{}) } -> std::same_as<Action>;
    { client.Process(instrs::PSRRead{}) } -> std::same_as<Action>;
    { client.Process(instrs::PSRWrite{}) } -> std::same_as<Action>;
    { client.Process(instrs::SingleDataTransfer{}) } -> std::same_as<Action>;
    { client.Process(instrs::HalfwordAndSignedTransfer{}) } -> std::same_as<Action>;
    { client.Process(instrs::BlockTransfer{}) } -> std::same_as<Action>;
    { client.Process(instrs::SingleDataSwap{}) } -> std::same_as<Action>;
    { client.Process(instrs::SoftwareInterrupt{}) } -> std::same_as<Action>;
    { client.Process(instrs::SoftwareBreakpoint{}) } -> std::same_as<Action>;
    { client.Process(instrs::Preload{}) } -> std::same_as<Action>;
    { client.Process(instrs::CopDataOperations{}) } -> std::same_as<Action>;
    { client.Process(instrs::CopDataTransfer{}) } -> std::same_as<Action>;
    { client.Process(instrs::CopRegTransfer{}) } -> std::same_as<Action>;
    { client.Process(instrs::CopDualRegTransfer{}) } -> std::same_as<Action>;
    { client.Process(instrs::Undefined{}) } -> std::same_as<Action>;
};

} // namespace armajitto::arm::decoder
