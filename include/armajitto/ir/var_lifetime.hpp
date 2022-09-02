#pragma once

#include "basic_block.hpp"
#include "ir_ops.hpp"

#include <vector>

namespace armajitto::ir {

class VarLifetimeTracker {
public:
    void Analyze(const ir::BasicBlock &block);

    bool IsEndOfLife(ir::Variable var, const ir::IROp *op) const;

private:
    std::vector<const ir::IROp *> m_lastVarUseOps;

    void SetLastVarUseOp(ir::Variable var, const ir::IROp *op);
};

} // namespace armajitto::ir
