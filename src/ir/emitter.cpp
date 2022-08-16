#include "armajitto/ir/emitter.hpp"

namespace armajitto::ir {

Variable Emitter::CreateVariable(const char *name) {
    return vars.emplace_back(vars.size(), name);
}

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

void Emitter::CountLeadingZeros(VariableArg dst, VarOrImmArg value) {
    auto *op = new IRCountLeadingZerosOp();
    op->dst = dst;
    op->value = value;
    ops.push_back(op);
}

} // namespace armajitto::ir