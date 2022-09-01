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

    int64_t Call(LocationRef loc, uint64_t cycles) final {
        auto code = GetCodeForLocation(loc);
        return Call(code, cycles);
    }

    int64_t Call(HostCode code, uint64_t cycles) final {
        if (code != 0) {
            return m_compiledCode.prolog(code, cycles);
        } else {
            return cycles;
        }
    }

    void Clear() final;

private:
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

    struct XbyakCodeGen : public Xbyak::CodeGenerator {
        using Xbyak::CodeGenerator::CodeGenerator;

        void resetAndReallocate(size_t size = Xbyak::DEFAULT_MAX_CODE_SIZE) {
            setProtectModeRW();
            reset();
            maxSize_ = 0;
            growMemory();
        }
    };

    XbyakAllocator m_alloc;
    XbyakCodeGen m_codegen;
    CompiledCode m_compiledCode;

    struct Compiler;

    void CompileCommon();

    void CompileProlog();
    void CompileEpilog();
    void CompileIRQEntry();
};

} // namespace armajitto::x86_64
