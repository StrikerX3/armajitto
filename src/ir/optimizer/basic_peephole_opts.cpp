#include "basic_peephole_opts.hpp"

namespace armajitto::ir {

void BasicPeepholeOptimizerPass::ResizeValues(size_t index) {
    if (m_values.size() <= index) {
        m_values.resize(index + 1);
    }
}

void BasicPeepholeOptimizerPass::AssignConstant(Variable var, uint32_t value) {
    if (!var.IsPresent()) {
        return;
    }
    const auto index = var.Index();
    ResizeValues(index);
    m_values[index].valid = true;
    m_values[index].knownBits = ~0;
    m_values[index].value = value;
}

void BasicPeepholeOptimizerPass::CopyVariable(Variable var, Variable src, IROp *op) {
    if (!var.IsPresent()) {
        return;
    }
    if (!src.IsPresent()) {
        return;
    }
    const auto srcIndex = src.Index();
    if (srcIndex >= m_values.size()) {
        return;
    }
    const auto dstIndex = var.Index();
    ResizeValues(dstIndex);
    m_values[dstIndex] = m_values[srcIndex];
    m_values[dstIndex].source = src;
    m_values[dstIndex].writerOp = op;
}

void BasicPeepholeOptimizerPass::DeriveKnownBits(Variable var, Variable src, uint32_t mask, uint32_t value, IROp *op) {
    if (!var.IsPresent()) {
        return;
    }
    if (!src.IsPresent()) {
        return;
    }
    const auto srcIndex = src.Index();
    if (srcIndex >= m_values.size()) {
        return;
    }
    const auto dstIndex = var.Index();
    ResizeValues(dstIndex);

    auto &dstValues = m_values[dstIndex];
    auto &srcValues = m_values[srcIndex];

    dstValues.valid = true;
    dstValues.source = src;
    dstValues.writerOp = op;
    if (srcValues.valid) {
        dstValues.knownBits = srcValues.knownBits | mask;
        dstValues.value = (srcValues.value & ~mask) | (value & mask);
    } else {
        dstValues.knownBits = mask;
        dstValues.value = value & mask;
    }
}

void BasicPeepholeOptimizerPass::Process(IRLogicalShiftLeftOp *op) {}

void BasicPeepholeOptimizerPass::Process(IRLogicalShiftRightOp *op) {}

void BasicPeepholeOptimizerPass::Process(IRArithmeticShiftRightOp *op) {}

void BasicPeepholeOptimizerPass::Process(IRRotateRightOp *op) {}

void BasicPeepholeOptimizerPass::Process(IRRotateRightExtendOp *op) {}

void BasicPeepholeOptimizerPass::Process(IRBitwiseAndOp *op) {}

void BasicPeepholeOptimizerPass::Process(IRBitwiseOrOp *op) {}

void BasicPeepholeOptimizerPass::Process(IRBitwiseXorOp *op) {}

void BasicPeepholeOptimizerPass::Process(IRBitClearOp *op) {}

void BasicPeepholeOptimizerPass::Process(IRCountLeadingZerosOp *op) {}

void BasicPeepholeOptimizerPass::Process(IRAddOp *op) {}

void BasicPeepholeOptimizerPass::Process(IRAddCarryOp *op) {}

void BasicPeepholeOptimizerPass::Process(IRSubtractOp *op) {}

void BasicPeepholeOptimizerPass::Process(IRSubtractCarryOp *op) {}

void BasicPeepholeOptimizerPass::Process(IRMoveOp *op) {}

void BasicPeepholeOptimizerPass::Process(IRMoveNegatedOp *op) {}

void BasicPeepholeOptimizerPass::Process(IRConstantOp *op) {}

void BasicPeepholeOptimizerPass::Process(IRCopyVarOp *op) {}

} // namespace armajitto::ir
