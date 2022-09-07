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

#include <memory_resource>

namespace armajitto::x86_64 {

class x64Host final : public Host {
public:
    static constexpr size_t kDefaultMaxCodeSize = static_cast<size_t>(1) * 1024 * 1024;

    x64Host(Context &context, std::pmr::memory_resource &alloc, size_t maxCodeSize = kDefaultMaxCodeSize);
    ~x64Host();

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

    void InvalidateCodeCache() final;
    void InvalidateCodeCacheRange(uint32_t start, uint32_t end) final;

private:
    struct CustomCodeGenerator : public Xbyak::CodeGenerator {
    public:
        using Xbyak::CodeGenerator::CodeGenerator;

        void setCodeBuffer(uint8_t *codeBuffer, size_t size) {
            top_ = codeBuffer;
            maxSize_ = size;
            reset();
        }
    };

    std::unique_ptr<uint8_t[]> m_codeBuffer;
    size_t m_codeBufferSize;
    CustomCodeGenerator m_codegen;
    CompiledCode m_compiledCode;
    std::pmr::memory_resource &m_alloc;

    struct Compiler;

    void CompileCommon();

    void CompileProlog();
    void CompileEpilog();
    void CompileIRQEntry();

    HostCode CompileImpl(ir::BasicBlock &block);

    void ApplyDirectLinkPatches(LocationRef target, HostCode blockCode);
    void RevertDirectLinkPatches(uint64_t target);
};

} // namespace armajitto::x86_64
