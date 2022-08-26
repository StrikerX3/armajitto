#pragma once

#include "armajitto/host/x86_64/x86_64_host.hpp"

#include "reg_alloc.hpp"

#include <xbyak/xbyak.h>

namespace armajitto::x86_64 {

struct x64Host::Compiler {
    Compiler(Xbyak::CodeGenerator &code)
        : regAlloc(code) {}

    RegisterAllocator regAlloc;
};

} // namespace armajitto::x86_64
