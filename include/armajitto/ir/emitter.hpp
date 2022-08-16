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
    void LoadCPSR(VariableArg dst);
    void StoreCPSR(VarOrImmArg src);
    void LoadSPSR(arm::Mode mode, VariableArg dst);
    void StoreSPSR(arm::Mode mode, VarOrImmArg src);

    void MemRead(MemAccessMode mode, MemAccessSize size, VariableArg dst, VarOrImmArg address);
    void MemWrite(MemAccessSize size, VarOrImmArg src, VarOrImmArg address);

    void CountLeadingZeros(VariableArg dst, VarOrImmArg value);

private:
    std::vector<IROp *> ops; // TODO: avoid raw pointers
    std::vector<Variable> vars;
};

} // namespace armajitto::ir
