#include "armajitto/ir/emitter.hpp"
#include "armajitto/ir/ops/ir_ops.hpp"

namespace armajitto::ir {

Variable Emitter::Var(const char *name) {
    return m_block.m_vars.emplace_back(m_block.m_vars.size(), name);
}

void Emitter::SetCondition(arm::Condition cond) {
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

} // namespace armajitto::ir
