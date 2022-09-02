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
    static constexpr size_t kDefaultMaxCodeSize = static_cast<size_t>(32) * 1024 * 1024;

    x64Host(Context &context, size_t maxCodeSize = kDefaultMaxCodeSize);
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
            ProtectRE();
            return m_compiledCode.prolog(code, cycles);
        } else {
            return cycles;
        }
    }

    void Clear() final;

private:
    std::unique_ptr<uint8_t[]> m_codeBuffer;
    Xbyak::CodeGenerator m_codegen;
    CompiledCode m_compiledCode;

    struct Compiler;

    void CompileCommon();

    void CompileProlog();
    void CompileEpilog();
    void CompileIRQEntry();

    // -----------------------------------------------------------------------------------------------------------------
    // Memory protection control

    bool m_isExecutable;

    void ProtectRW();
    void ProtectRE();
};

} // namespace armajitto::x86_64
