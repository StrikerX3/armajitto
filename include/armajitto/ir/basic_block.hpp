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
    IRCodeFragment *CreateCodeFragment();

private:
    std::vector<IRCodeFragment> m_codeFragments;
};

} // namespace armajitto::ir
