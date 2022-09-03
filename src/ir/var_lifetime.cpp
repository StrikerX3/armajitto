#include "armajitto/ir/var_lifetime.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

namespace armajitto::ir {

VarLifetimeTracker::VarLifetimeTracker(std::pmr::memory_resource &alloc)
    : m_lastVarUseOps(&alloc) {}

void VarLifetimeTracker::Analyze(const ir::BasicBlock &block) {
    m_lastVarUseOps.clear();
    m_lastVarUseOps.resize(block.VariableCount());

    auto *op = block.Head();
    while (op != nullptr) {
        ir::VisitIROpVars(op,
                          [this](const auto *op, ir::Variable var, bool read) -> void { SetLastVarUseOp(var, op); });
        op = op->Next();
    }
}

bool VarLifetimeTracker::IsEndOfLife(ir::Variable var, const ir::IROp *op) const {
    if (!var.IsPresent()) {
        return false;
    }
    const auto varIndex = var.Index();
    if (var.Index() >= m_lastVarUseOps.size()) {
        return false;
    }
    return m_lastVarUseOps[varIndex] == op;
}

// ---------------------------------------------------------------------------------------------------------------------

void VarLifetimeTracker::SetLastVarUseOp(ir::Variable var, const ir::IROp *op) {
    const auto index = var.Index();
    m_lastVarUseOps[index] = op;
}

} // namespace armajitto::ir
