#include "var_lifetime.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

namespace armajitto::x86_64 {

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
    return m_lastVarUseOps[var.Index()] == op;
}

// ---------------------------------------------------------------------------------------------------------------------

void VarLifetimeTracker::SetLastVarUseOp(ir::Variable var, const ir::IROp *op) {
    const auto index = var.Index();
    m_lastVarUseOps[index] = op;
}

} // namespace armajitto::x86_64