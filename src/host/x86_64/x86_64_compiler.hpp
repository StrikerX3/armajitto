#pragma once

#include "armajitto/host/x86_64/x86_64_host.hpp"

#include "reg_alloc.hpp"

#include <xbyak/xbyak.h>

namespace armajitto::x86_64 {

struct x64Host::Compiler {
    Compiler(Xbyak::CodeGenerator &codegen)
        : codegen(codegen)
        , regAlloc(codegen) {}

    void Analyze(const ir::BasicBlock &block) {
        regAlloc.Analyze(block);
        mode = block.Location().Mode();
        thumb = block.Location().IsThumbMode();
    }

    void PreProcessOp(const ir::IROp *op);
    void PostProcessOp(const ir::IROp *op);

    Xbyak::CodeGenerator &codegen;
    RegisterAllocator regAlloc;
    arm::Mode mode;
    bool thumb;
};

} // namespace armajitto::x86_64
