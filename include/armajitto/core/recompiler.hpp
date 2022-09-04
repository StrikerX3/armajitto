#pragma once

#include "armajitto/core/allocator.hpp"
#include "armajitto/core/pmr_allocator.hpp"
#include "armajitto/guest/arm/state.hpp"
#include "armajitto/host/x86_64/x86_64_host.hpp" // TODO: select based on host system
#include "armajitto/ir/optimizer.hpp"
#include "armajitto/ir/translator.hpp"
#include "context.hpp"
#include "specification.hpp"

namespace armajitto {

class Recompiler {
public:
    Recompiler(const Specification &spec)
        : m_spec(spec)
        , m_context(spec.model, spec.system)
        , m_pmrAllocator(m_allocator)
        , m_translator(m_context)
        , m_host(m_context, m_pmrAllocator, spec.maxHostCodeSize) {}

    arm::State &GetARMState() {
        return m_context.GetARMState();
    }

    CPUModel GetCPUModel() const {
        return m_spec.model;
    }

    CPUArch GetCPUArch() const {
        return m_context.GetCPUArch();
    }

    ISystem &GetSystem() {
        return m_spec.system;
    }

    ir::Translator::Parameters &GetTranslatorParameters() {
        return m_translator.GetParameters();
    }

    ir::OptimizationParams &GetOptimizationParameters() {
        return m_optParams;
    }

    uint64_t Run(uint64_t minCycles);

    void FlushCachedBlocks();

private:
    Specification m_spec;
    Context m_context;

    memory::Allocator m_allocator;
    memory::PMRAllocatorWrapper m_pmrAllocator;

    ir::Translator m_translator;
    ir::OptimizationParams m_optParams;

    // TODO: select based on host system
    x86_64::x64Host m_host;

    uint32_t m_compiledBlocks = 0;
    static constexpr uint32_t kCompiledBlocksReleaseThreshold = 500;
};

} // namespace armajitto
