#include "armajitto/core/recompiler.hpp"

#include "host/interp/interp_host.hpp" // TODO: select based on host system
#include "host/x86_64/x86_64_host.hpp" // TODO: select based on host system

#include "core/allocator.hpp"

#include "ir/optimizer.hpp"
#include "ir/translator.hpp"
#include "ir/verifier.hpp"

#include <memory_resource>

namespace armajitto {

struct Recompiler::Impl {
    Impl(Context &context, Specification spec, Options &params)
        : context(context)
        , translator(context, params.translator)
        , optimizer(context, params.optimizer, pmrBuffer)
        , host(context, params.compiler, spec.cycleCountDeadline, pmrBuffer) {}

    void Reset() {
        FlushCachedBlocks();
        context.GetARMState().Reset();
    }

    uint64_t Run(uint64_t initialCycles) {
        auto &armState = context.GetARMState();
        uint32_t &pc = armState.GPR(arm::GPR::PC);

        const bool hasDeadline = armState.deadlinePtr != nullptr;

        uint64_t cycles = initialCycles;
        while (hasDeadline ? (cycles < *armState.deadlinePtr) : ((int64_t)cycles > 0)) {
            // Build location reference and get its code
            const LocationRef loc{pc, armState.CPSR().u32};
            auto code = host.GetCodeForLocation(loc);

            // Compile code if not yet compiled
            if (code == nullptr) {
                // Invalidate block at the specified location.
                // This should clean up pending patches and undo applied patches.
                host.Invalidate(loc);

                // Compile the new block
                auto *block = allocator.Allocate<ir::BasicBlock>(allocator, loc);
                translator.Translate(*block);
                optimizer.Optimize(*block);
                verifier.Verify(*block);
                code = host.Compile(*block);

                // Cleanup
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
            auto nextCycles = host.Call(code, cycles);
            if (nextCycles == cycles) {
                // CPU is halted and no IRQs were raised
                break;
            }
            cycles = nextCycles;
        }
        return hasDeadline ? (cycles - initialCycles) : (initialCycles - cycles);
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

    void ReportMemoryWrite(uint32_t start, uint32_t end) {
        host.ReportMemoryWrite(start, end);
    }

    memory::Allocator allocator;
    // std::pmr::monotonic_buffer_resource pmrBuffer{std::pmr::get_default_resource()};
    std::pmr::unsynchronized_pool_resource pmrBuffer{std::pmr::get_default_resource()};

    Context &context;
    ir::Translator translator;
    ir::Optimizer optimizer;
    ir::Verifier verifier;

    // TODO: select based on host system
    x86_64::x64Host host;
    // interp::InterpreterHost host;

    uint32_t compiledBlocks = 0;
    static constexpr uint32_t kCompiledBlocksReleaseThreshold = 500;
};

Recompiler::Recompiler(Specification spec)
    : m_spec(spec)
    , m_context(spec.model, spec.system)
    , m_impl(std::make_unique<Impl>(m_context, spec, m_options)) {}

Recompiler::~Recompiler() = default;

void Recompiler::Reset() {
    m_impl->Reset();
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

void Recompiler::ReportMemoryWrite(uint32_t start, uint32_t end) {
    m_impl->ReportMemoryWrite(start, end);
}

} // namespace armajitto
