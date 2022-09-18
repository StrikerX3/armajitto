#pragma once

#include "host/host.hpp"

#include "ir/defs/arguments.hpp"
#include "ir/ir_ops.hpp"

#include "util/pointer_cast.hpp"
#include "util/type_traits.hpp"

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
    x64Host(Context &context, Options::Compiler &options, std::pmr::memory_resource &alloc);
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

    class Compiler;
    struct CommonData;

    std::unique_ptr<CommonData> m_commonData;
    std::unique_ptr<uint8_t[]> m_codeBuffer;
    size_t m_codeBufferSize;
    CustomCodeGenerator m_codegen;
    CompiledCode m_compiledCode;
    std::pmr::memory_resource &m_alloc;

    void CompileCommon();

    void CompileProlog();
    void CompileEpilog();
    void CompileIRQEntry();

    HostCode CompileImpl(ir::BasicBlock &block);

    void ApplyDirectLinkPatches(LocationRef target, HostCode blockCode);
    void RevertDirectLinkPatches(uint64_t target);
};

} // namespace armajitto::x86_64
