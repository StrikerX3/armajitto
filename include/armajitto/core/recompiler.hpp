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
        , m_translator(m_context)
        , m_optimizer(m_pmrBuffer)
        , m_host(m_context, m_pmrBuffer, spec.maxHostCodeSize) {}

    void Reset();

    arm::State &GetARMState() {
        return m_context.GetARMState();
    }

    const arm::State &GetARMState() const {
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

    ir::Optimizer::Parameters &GetOptimizationParameters() {
        return m_optimizer.GetParameters();
    }

    uint64_t Run(uint64_t minCycles);

    void FlushCachedBlocks();

private:
    Specification m_spec;
    Context m_context;

    memory::Allocator m_allocator;
    // std::pmr::monotonic_buffer_resource m_pmrBuffer{std::pmr::get_default_resource()};
    std::pmr::unsynchronized_pool_resource m_pmrBuffer{std::pmr::get_default_resource()};

    ir::Translator m_translator;
    ir::Optimizer m_optimizer;

    // TODO: select based on host system
    x86_64::x64Host m_host;

    uint32_t m_compiledBlocks = 0;
    static constexpr uint32_t kCompiledBlocksReleaseThreshold = 500;
};

} // namespace armajitto
