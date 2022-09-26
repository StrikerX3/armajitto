#pragma once

#include "armajitto/guest/arm/state.hpp"
#include "context.hpp"
#include "options.hpp"
#include "specification.hpp"

#include <memory>

namespace armajitto {

class Recompiler {
public:
    Recompiler(Specification spec);
    ~Recompiler();

    void Reset();

    Options &GetOptions() {
        return m_options;
    }

    CPUModel GetCPUModel() const {
        return m_spec.model;
    }

    CPUArch GetCPUArch() const {
        return m_context.GetCPUArch();
    }

    arm::State &GetARMState() {
        return m_context.GetARMState();
    }

    ISystem &GetSystem() {
        return m_spec.system;
    }

    const arm::State &GetARMState() const {
        return const_cast<Recompiler *>(this)->GetARMState();
    }

    const ISystem &GetSystem() const {
        return const_cast<Recompiler *>(this)->GetSystem();
    }

    uint64_t Run(uint64_t minCycles);

    void FlushCachedBlocks();
    void InvalidateCodeCache();
    void InvalidateCodeCacheRange(uint32_t start, uint32_t end);

    void ReportMemoryWrite(uint32_t start, uint32_t end);

private:
    Specification m_spec;
    Context m_context;
    Options m_options;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace armajitto
