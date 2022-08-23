#include "arithmetic_ops_coalescence.hpp"

namespace armajitto::ir {

ArithmeticOpsCoalescenceOptimizerPass::ArithmeticOpsCoalescenceOptimizerPass(Emitter &emitter)
    : OptimizerPassBase(emitter) {

    // TODO: resize vectors if needed
    // const uint32_t varCount = emitter.VariableCount();
}

} // namespace armajitto::ir
