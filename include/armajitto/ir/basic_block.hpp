#pragma once

#include "armajitto/ir/emitter.hpp"

namespace armajitto::ir {

struct IRCodeFragment {
    Emitter emitter;
};

class BasicBlock {
public:
    IRCodeFragment *CreateCodeFragment();

private:
    std::vector<IRCodeFragment> m_codeFragments;
};

} // namespace armajitto::ir
