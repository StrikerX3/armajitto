#include "x86_64_compiler.hpp"

namespace armajitto::x86_64 {

void x64Host::Compiler::PreProcessOp(const ir::IROp *op) {
    regAlloc.SetInstruction(op);
}

void x64Host::Compiler::PostProcessOp(const ir::IROp *op) {
    regAlloc.ReleaseVars();
    regAlloc.ReleaseTemporaries();
}

} // namespace armajitto::x86_64
