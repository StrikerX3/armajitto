#include "armajitto/ir/basic_block.hpp"

namespace armajitto::ir {

IRCodeFragment *BasicBlock::CreateCodeFragment() {
    return &m_codeFragments.emplace_back();
}

} // namespace armajitto::ir
