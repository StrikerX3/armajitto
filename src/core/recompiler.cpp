#include "armajitto/core/recompiler.hpp"

namespace armajitto {

void Recompiler::Reset() {
    FlushCachedBlocks();
    m_context.GetARMState().Reset();
}

uint64_t Recompiler::Run(uint64_t minCycles) {
    auto &armState = m_context.GetARMState();
    uint32_t &pc = armState.GPR(arm::GPR::PC);
    int64_t cyclesRemaining = minCycles;
    while (cyclesRemaining > 0) {
        // Build location reference and get its code
        const LocationRef loc{pc, armState.CPSR().u32};
        auto code = m_host.GetCodeForLocation(loc);

        // Compile code if not yet compiled
        if (code == nullptr) {
            auto *block = m_allocator.Allocate<ir::BasicBlock>(m_allocator, loc);
            m_translator.Translate(*block);
            m_optimizer.Optimize(*block);
            code = m_host.Compile(*block);
            if constexpr (ir::BasicBlock::kFreeErasedIROps) {
                block->Clear();
                m_allocator.Free(block);
            } else {
                if (++m_compiledBlocks == kCompiledBlocksReleaseThreshold) {
                    m_compiledBlocks = 0;
                    m_allocator.Release();
                    m_pmrBuffer.release();
                }
            }
        }

        // Invoke code
        cyclesRemaining = m_host.Call(code, cyclesRemaining);
    }
    return minCycles - cyclesRemaining;
}

void Recompiler::FlushCachedBlocks() {
    m_host.Clear();
    m_allocator.Release();
    m_pmrBuffer.release();
    m_compiledBlocks = 0;
}

} // namespace armajitto
