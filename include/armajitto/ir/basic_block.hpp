#pragma once

#include "armajitto/defs/arm/instructions.hpp"
#include "armajitto/ir/emitter.hpp"

namespace armajitto::ir {

struct IRCodeFragment {
    Emitter emitter;
    arm::Condition cond;
};

class BasicBlock {
public:
    BasicBlock(uint32_t baseAddress, arm::Mode mode, bool thumb)
        : m_baseAddress(baseAddress)
        , m_mode(mode)
        , m_thumb(thumb) {}

    IRCodeFragment *CreateCodeFragment();

    uint32_t BaseAddress() const {
        return m_baseAddress;
    }

    arm::Mode Mode() const {
        return m_mode;
    }

    bool IsThumbMode() const {
        return m_thumb;
    }

private:
    uint32_t m_baseAddress;
    arm::Mode m_mode;
    bool m_thumb;

    std::vector<IRCodeFragment> m_codeFragments;
};

} // namespace armajitto::ir
