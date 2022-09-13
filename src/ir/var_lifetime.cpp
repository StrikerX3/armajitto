#include "var_lifetime.hpp"

#include "ops/ir_ops_visitor.hpp"

namespace armajitto::ir {

VarLifetimeTracker::VarLifetimeTracker(std::pmr::memory_resource &alloc)
    : m_lastVarUseOps(&alloc) {}

void VarLifetimeTracker::Analyze(const ir::BasicBlock &block) {
    m_lastVarUseOps.clear();
    m_lastVarUseOps.resize(block.VariableCount());
    m_varExpired.resize(block.VariableCount());
    std::fill(m_varExpired.begin(), m_varExpired.end(), false);

    auto *op = block.Head();
    while (op != nullptr) {
        ir::VisitIROpVars(op,
                          [this](const auto *op, ir::Variable var, bool read) -> void { SetLastVarUseOp(var, op); });
        op = op->Next();
    }
}

void VarLifetimeTracker::Update(const ir::IROp *op) {
    ir::VisitIROpVars(op, [this](const auto *op, ir::Variable var, bool read) -> void {
        if (!var.IsPresent()) {
            return;
        }
        auto varIndex = var.Index();
        if (varIndex >= m_lastVarUseOps.size()) {
            return;
        }
        if (m_lastVarUseOps[varIndex] == op) {
            m_varExpired[varIndex] = true;
        }
    });
}

bool VarLifetimeTracker::IsEndOfLife(ir::Variable var, const ir::IROp *op) const {
    if (!var.IsPresent()) {
        return false;
    }
    const auto varIndex = var.Index();
    if (varIndex >= m_lastVarUseOps.size()) {
        return false;
    }
    return m_lastVarUseOps[varIndex] == op;
}

bool VarLifetimeTracker::IsExpired(ir::Variable var) const {
    if (!var.IsPresent()) {
        return false;
    }
    const auto varIndex = var.Index();
    if (varIndex >= m_lastVarUseOps.size()) {
        return false;
    }
    return m_varExpired[varIndex];
}

// ---------------------------------------------------------------------------------------------------------------------

void VarLifetimeTracker::SetLastVarUseOp(ir::Variable var, const ir::IROp *op) {
    const auto index = var.Index();
    m_lastVarUseOps[index] = op;
}

} // namespace armajitto::ir
