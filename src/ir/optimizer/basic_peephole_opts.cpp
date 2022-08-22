#include "basic_bitwise_peephole_opts.hpp"

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

void BasicPeepholeOptimizerPass::CopyVariable(Variable var, Variable src) {
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
}

void BasicPeepholeOptimizerPass::DeriveKnownBits(Variable var, Variable src, uint32_t mask, uint32_t value) {
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
    if (srcValues.valid) {
        dstValues.knownBits = srcValues.knownBits | mask;
        dstValues.value = (srcValues.value & ~mask) | (value & mask);
    } else {
        dstValues.knownBits = mask;
        dstValues.value = value & mask;
    }
}

} // namespace armajitto::ir
