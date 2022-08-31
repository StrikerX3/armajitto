#pragma once

#include "armajitto/host/host.hpp"
#include "armajitto/ir/defs/arguments.hpp"
#include "armajitto/ir/ir_ops.hpp"
#include "armajitto/util/pointer_cast.hpp"
#include "armajitto/util/type_traits.hpp"

#include "x86_64_compiled_code.hpp"
#include "x86_64_type_traits.hpp"

#ifdef _WIN32
    #define NOMINMAX
#endif
#include <xbyak/xbyak.h>

namespace armajitto::x86_64 {

class x64Host final : public Host {
public:
    x64Host(Context &context);

    HostCode Compile(ir::BasicBlock &block) final;

    HostCode GetCodeForLocation(LocationRef loc) final {
        return m_compiledCode.GetCodeForLocation(loc);
    }

    void Call(LocationRef loc) final {
        auto code = GetCodeForLocation(loc);
        Call(code);
    }

    void Call(HostCode code) final {
        if (code != 0) {
            m_prolog(code);
        }
    }

private:
    using PrologFn = void (*)(uintptr_t blockFn);
    PrologFn m_prolog;
    HostCode m_epilog;
    uint64_t m_stackAlignmentOffset;

    struct XbyakAllocator final : Xbyak::Allocator {
        using Xbyak::Allocator::Allocator;

        virtual uint8_t *alloc(size_t size) {
            return reinterpret_cast<uint8_t *>(allocator.AllocateRaw(size, 4096u));
        }

        virtual void free(uint8_t *p) {
            allocator.Free(p);
        }

        memory::Allocator allocator;
    };

    XbyakAllocator m_alloc;
    Xbyak::CodeGenerator m_codegen;
    CompiledCode m_compiledCode;

    struct Compiler;

    void CompileProlog();
    void CompileEpilog();
};

} // namespace armajitto::x86_64
