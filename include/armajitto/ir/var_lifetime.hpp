#pragma once

#include "basic_block.hpp"
#include "ir_ops.hpp"

#include <memory_resource>
#include <vector>

namespace armajitto::ir {

class VarLifetimeTracker {
public:
    VarLifetimeTracker(std::pmr::memory_resource &alloc);

    void Analyze(const ir::BasicBlock &block);

    bool IsEndOfLife(ir::Variable var, const ir::IROp *op) const;

private:
    std::pmr::vector<const ir::IROp *> m_lastVarUseOps;

    void SetLastVarUseOp(ir::Variable var, const ir::IROp *op);
};

} // namespace armajitto::ir
