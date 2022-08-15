#include "armajitto/ir/emitter.hpp"

namespace armajitto::ir {

Variable Emitter::CreateVariable(const char *name) {
    return vars.emplace_back(vars.size(), name);
}

void Emitter::EmitLoadGPR(VariableArg dst, GPRArg src) {
    auto *op = new IRLoadGPROp();
    op->dst = dst;
    op->src = src;
    ops.push_back(op);
}

void Emitter::EmitStoreGPR(GPRArg dst, VarOrImmArg src) {
    auto *op = new IRStoreGPROp();
    op->dst = dst;
    op->src = src;
    ops.push_back(op);
}

void Emitter::EmitCountLeadingZeros(VariableArg dst, VarOrImmArg value) {
    auto *op = new IRCountLeadingZerosOp();
    op->dst = dst;
    op->value = value;
    ops.push_back(op);
}

} // namespace armajitto::ir