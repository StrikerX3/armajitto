#include "armajitto/ir/emitter.hpp"

namespace armajitto::ir {

Variable Emitter::CreateVariable(const char *name) {
    return vars.emplace_back(vars.size(), name);
}

// TODO: avoid new; use allocator
// TODO: feels like these things should be constructors in the op classes themselves
// TODO: how will the optimizer work if we can only insert instructions at the end of a fragment?

void Emitter::LoadGPR(VariableArg dst, GPRArg src) {
    auto *op = new IRLoadGPROp();
    op->dst = dst;
    op->src = src;
    ops.push_back(op);
}

void Emitter::StoreGPR(GPRArg dst, VarOrImmArg src) {
    auto *op = new IRStoreGPROp();
    op->dst = dst;
    op->src = src;
    ops.push_back(op);
}

void Emitter::LoadCPSR(VariableArg dst) {
    auto *op = new IRLoadCPSROp();
    op->dst = dst;
    ops.push_back(op);
}

void Emitter::StoreCPSR(VarOrImmArg src) {
    auto *op = new IRStoreCPSROp();
    op->src = src;
    ops.push_back(op);
}

void Emitter::LoadSPSR(arm::Mode mode, VariableArg dst) {
    auto *op = new IRLoadSPSROp();
    op->mode = mode;
    op->dst = dst;
    ops.push_back(op);
}

void Emitter::StoreSPSR(arm::Mode mode, VarOrImmArg src) {
    auto *op = new IRStoreSPSROp();
    op->mode = mode;
    op->src = src;
    ops.push_back(op);
}

void Emitter::MemRead(MemAccessMode mode, MemAccessSize size, VariableArg dst, VarOrImmArg address) {
    auto *op = new IRMemReadOp();
    op->mode = mode;
    op->size = size;
    op->dst = dst;
    op->address = address;
    ops.push_back(op);
}

void Emitter::MemWrite(MemAccessSize size, VarOrImmArg src, VarOrImmArg address) {
    auto *op = new IRMemWriteOp();
    op->size = size;
    op->src = src;
    op->address = address;
    ops.push_back(op);
}

void Emitter::CountLeadingZeros(VariableArg dst, VarOrImmArg value) {
    auto *op = new IRCountLeadingZerosOp();
    op->dst = dst;
    op->value = value;
    ops.push_back(op);
}

} // namespace armajitto::ir
