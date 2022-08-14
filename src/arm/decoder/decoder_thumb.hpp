#pragma once

#include "decoder_client.hpp"

namespace armajitto::arm::decoder {

enum class ThumbALUOp : uint8_t { AND, EOR, LSL, LSR, ASR, ADC, SBC, ROR, TST, NEG, CMP, CMN, ORR, MUL, BIC, MVN };

template <Client TClient>
void DecodeThumb(TClient &client, uint32_t address) {
    // TODO: implement
}

} // namespace armajitto::arm::decoder
