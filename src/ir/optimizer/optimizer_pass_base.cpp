#include "optimizer_pass_base.hpp"

#include "armajitto/ir/ops/ir_ops_visitor.hpp"

#include <bit>
#include <cassert>
#include <optional>
#include <utility>

namespace armajitto::ir {

bool OptimizerPassBase::Optimize() {
    m_emitter.ClearDirtyFlag();

    PreProcess();

    m_emitter.GoToHead();
    while (IROp *op = m_emitter.GetCurrentOp()) {
        VisitIROp(op, [this](auto op) -> void { Process(op); });
        m_emitter.NextOp();
    }

    PostProcess();

    return m_dirty || m_emitter.IsDirty();
}

} // namespace armajitto::ir
