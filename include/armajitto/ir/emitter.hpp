#pragma once

#include "armajitto/defs/cpu_arch.hpp"
#include "armajitto/ir/ops/ir_ops.hpp"
#include "defs/variable.hpp"

#include <vector>

namespace armajitto::ir {

class Emitter {
public:
    Variable CreateVariable(const char *name);

    void LoadGPR(VariableArg dst, GPRArg src);
    void StoreGPR(GPRArg dst, VarOrImmArg src);

    void CountLeadingZeros(VariableArg dst, VarOrImmArg value);

private:
    std::vector<IROpBase *> ops; // TODO: avoid raw pointers
    std::vector<Variable> vars;
};

} // namespace armajitto::ir
