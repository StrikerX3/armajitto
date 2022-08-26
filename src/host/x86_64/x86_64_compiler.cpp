#include "x86_64_compiler.hpp"

namespace armajitto::x86_64 {

void x64Host::Compiler::PostProcessOp(const ir::IROp *op) {
    regAlloc.ReleaseVars(op);
    regAlloc.ReleaseTemporaries();
}

} // namespace armajitto::x86_64
