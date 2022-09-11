#include "armajitto/core/recompiler.hpp"

#include "host/x86_64/x86_64_host.hpp" // TODO: select based on host system

#include "core/allocator.hpp"

#include "ir/optimizer.hpp"
#include "ir/translator.hpp"

#include <memory_resource>

namespace armajitto {

struct Recompiler::Impl {
    Impl(Context &context, const Specification &spec)
        : context(context)
        , translator(context)
        , optimizer(pmrBuffer)
        , host(context, pmrBuffer, spec.maxHostCodeSize) {}

    void Reset() {
        FlushCachedBlocks();
        context.GetARMState().Reset();
    }

    uint64_t Run(uint64_t minCycles) {
        auto &armState = context.GetARMState();
        uint32_t &pc = armState.GPR(arm::GPR::PC);
        int64_t cyclesRemaining = minCycles;
        while (cyclesRemaining > 0) {
            // Build location reference and get its code
            const LocationRef loc{pc, armState.CPSR().u32};
            auto code = host.GetCodeForLocation(loc);

            // Compile code if not yet compiled
            if (code == nullptr) {
                auto *block = allocator.Allocate<ir::BasicBlock>(allocator, loc);
                translator.Translate(*block);
                optimizer.Optimize(*block);
                code = host.Compile(*block);
                if constexpr (ir::BasicBlock::kFreeErasedIROps) {
                    block->Clear();
                    allocator.Free(block);
                } else {
                    if (++compiledBlocks == kCompiledBlocksReleaseThreshold) {
                        compiledBlocks = 0;
                        allocator.Release();
                        pmrBuffer.release();
                    }
                }
            }

            // Invoke code
            auto nextCyclesRemaining = host.Call(code, cyclesRemaining);
            if (nextCyclesRemaining == cyclesRemaining) {
                // CPU is halted and no IRQs were raised
                break;
            }
            cyclesRemaining = nextCyclesRemaining;
        }
        return minCycles - cyclesRemaining;
    }

    void FlushCachedBlocks() {
        host.Clear();
        allocator.Release();
        pmrBuffer.release();
        compiledBlocks = 0;
    }

    void InvalidateCodeCache() {
        host.InvalidateCodeCache();
    }

    void InvalidateCodeCacheRange(uint32_t start, uint32_t end) {
        host.InvalidateCodeCacheRange(start, end);
    }

    memory::Allocator allocator;
    // std::pmr::monotonic_buffer_resource pmrBuffer{std::pmr::get_default_resource()};
    std::pmr::unsynchronized_pool_resource pmrBuffer{std::pmr::get_default_resource()};

    Context &context;
    ir::Translator translator;
    ir::Optimizer optimizer;

    // TODO: select based on host system
    x86_64::x64Host host;

    uint32_t compiledBlocks = 0;
    static constexpr uint32_t kCompiledBlocksReleaseThreshold = 500;
};

Recompiler::Recompiler(const Specification &spec)
    : m_spec(spec)
    , m_context(spec.model, spec.system)
    , m_impl(std::make_unique<Impl>(m_context, spec)) {}

Recompiler::~Recompiler() = default;

void Recompiler::Reset() {
    m_impl->Reset();
}

TranslatorParameters &Recompiler::GetTranslatorParameters() {
    return m_impl->translator.GetParameters();
}

OptimizerParameters &Recompiler::GetOptimizerParameters() {
    return m_impl->optimizer.GetParameters();
}

uint64_t Recompiler::Run(uint64_t minCycles) {
    return m_impl->Run(minCycles);
}

void Recompiler::FlushCachedBlocks() {
    m_impl->FlushCachedBlocks();
}

void Recompiler::InvalidateCodeCache() {
    m_impl->InvalidateCodeCache();
}

void Recompiler::InvalidateCodeCacheRange(uint32_t start, uint32_t end) {
    m_impl->InvalidateCodeCacheRange(start, end);
}

} // namespace armajitto
