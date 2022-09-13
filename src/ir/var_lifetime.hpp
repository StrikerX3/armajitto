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
    void Update(const ir::IROp *op);

    bool IsEndOfLife(ir::Variable var, const ir::IROp *op) const;
    bool IsExpired(ir::Variable var) const;

private:
    std::pmr::vector<const ir::IROp *> m_lastVarUseOps;
    std::pmr::vector<bool> m_varExpired;

    void SetLastVarUseOp(ir::Variable var, const ir::IROp *op);
};

} // namespace armajitto::ir
