#include "armajitto/ir/emitter.hpp"
#include "armajitto/ir/ops/ir_ops.hpp"

namespace armajitto::ir {

Variable Emitter::Var(const char *name) {
    return m_block.m_vars.emplace_back(m_block.m_vars.size(), name);
}

void Emitter::NextInstruction(arm::Condition cond) {
    ++m_block.m_instrCount;
    m_block.m_cond = cond;
}

void Emitter::LoadGPR(VariableArg dst, GPRArg src) {
    AppendOp<IRLoadGPROp>(dst, src);
}

void Emitter::StoreGPR(GPRArg dst, VarOrImmArg src) {
    AppendOp<IRStoreGPROp>(dst, src);
}

void Emitter::LoadCPSR(VariableArg dst) {
    AppendOp<IRLoadCPSROp>(dst);
}

void Emitter::StoreCPSR(VarOrImmArg src) {
    AppendOp<IRStoreCPSROp>(src);
}

void Emitter::LoadSPSR(arm::Mode mode, VariableArg dst) {
    AppendOp<IRLoadSPSROp>(mode, dst);
}

void Emitter::StoreSPSR(arm::Mode mode, VarOrImmArg src) {
    AppendOp<IRStoreSPSROp>(mode, src);
}

void Emitter::MemRead(MemAccessMode mode, MemAccessSize size, VariableArg dst, VarOrImmArg address) {
    AppendOp<IRMemReadOp>(mode, size, dst, address);
}

void Emitter::MemWrite(MemAccessSize size, VarOrImmArg src, VarOrImmArg address) {
    AppendOp<IRMemWriteOp>(size, src, address);
}

void Emitter::CountLeadingZeros(VariableArg dst, VarOrImmArg value) {
    AppendOp<IRCountLeadingZerosOp>(dst, value);
}

void Emitter::InstructionFetch() {
    const auto &loc = m_block.Location();
    const bool thumb = loc.IsThumbMode();
    const uint32_t fetchAddress =
        loc.BaseAddress() + (2 + m_block.m_instrCount) * (thumb ? sizeof(uint16_t) : sizeof(uint32_t));

    StoreGPR(15, fetchAddress);
    // TODO: cycle counting
}

} // namespace armajitto::ir
