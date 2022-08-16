#include "armajitto/ir/emitter.hpp"

namespace armajitto::ir {

Variable Emitter::CreateVariable(const char *name) {
    return vars.emplace_back(vars.size(), name);
}

// TODO: avoid new; use allocator
// TODO: how will the optimizer work if we can only insert instructions at the end of a fragment?

void Emitter::LoadGPR(VariableArg dst, GPRArg src) {
    ops.push_back(new IRLoadGPROp(dst, src));
}

void Emitter::StoreGPR(GPRArg dst, VarOrImmArg src) {
    ops.push_back(new IRStoreGPROp(dst, src));
}

void Emitter::LoadCPSR(VariableArg dst) {
    ops.push_back(new IRLoadCPSROp(dst));
}

void Emitter::StoreCPSR(VarOrImmArg src) {
    ops.push_back(new IRStoreCPSROp(src));
}

void Emitter::LoadSPSR(arm::Mode mode, VariableArg dst) {
    ops.push_back(new IRLoadSPSROp(mode, dst));
}

void Emitter::StoreSPSR(arm::Mode mode, VarOrImmArg src) {
    ops.push_back(new IRStoreSPSROp(mode, src));
}

void Emitter::MemRead(MemAccessMode mode, MemAccessSize size, VariableArg dst, VarOrImmArg address) {
    ops.push_back(new IRMemReadOp(mode, size, dst, address));
}

void Emitter::MemWrite(MemAccessSize size, VarOrImmArg src, VarOrImmArg address) {
    ops.push_back(new IRMemWriteOp(size, src, address));
}

void Emitter::CountLeadingZeros(VariableArg dst, VarOrImmArg value) {
    ops.push_back(new IRCountLeadingZerosOp(dst, value));
}

} // namespace armajitto::ir
