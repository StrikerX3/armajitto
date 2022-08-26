#pragma once

#include "armajitto/ir/basic_block.hpp"
#include "armajitto/ir/ir_ops.hpp"

#include <vector>

namespace armajitto::x86_64 {

class VarLifetimeTracker {
public:
    void Analyze(const ir::BasicBlock &block);

    bool IsEndOfLife(ir::Variable var, const ir::IROp *op) const;

private:
    std::vector<const ir::IROp *> m_lastVarUseOps;

    void SetLastVarUseOp(ir::Variable var, const ir::IROp *op);
};

} // namespace armajitto::x86_64
