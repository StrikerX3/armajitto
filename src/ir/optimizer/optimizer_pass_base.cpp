#include "optimizer_pass_base.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

#include <bit>
#include <cassert>
#include <optional>
#include <utility>

namespace armajitto::ir {

bool OptimizerPassBase::Optimize() {
    m_dirty = false;
    m_emitter.ClearDirtyFlag();
    if (m_backward) {
        m_emitter.GoToTail();
    } else {
        m_emitter.GoToHead();
    }

    Reset();

    PreProcess();
    while (IROp *op = m_emitter.GetCurrentOp()) {
        PreProcess(op);
        VisitIROp(op, [this](auto op) -> void { Process(op); });
        if (!m_emitter.WasCurrentOpErased()) {
            PostProcess(op);
        }

        if (m_backward) {
            m_emitter.PrevOp();
        } else {
            m_emitter.NextOp();
        }
    }
    PostProcess();

    const bool dirty = m_dirty || m_emitter.IsDirty();
    if (dirty) {
        m_emitter.RenameVariables();
    }
    return dirty;
}

} // namespace armajitto::ir
