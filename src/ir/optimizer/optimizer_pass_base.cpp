#include "optimizer_pass_base.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

#include <bit>
#include <cassert>
#include <optional>
#include <utility>

namespace armajitto::ir {

bool OptimizerPassBase::Optimize() {
    m_emitter.ClearDirtyFlag();
    m_emitter.GoToHead();

    PreProcess();
    while (IROp *op = m_emitter.GetCurrentOp()) {
        VisitIROp(op, [this](auto op) -> void { Process(op); });
        m_emitter.NextOp();
    }
    PostProcess();

    m_emitter.RenameVariables();
    return m_dirty || m_emitter.IsDirty();
}

} // namespace armajitto::ir
