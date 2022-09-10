#pragma once

#include "armajitto/guest/arm/state.hpp"
#include "context.hpp"
#include "params.hpp"
#include "specification.hpp"

#include <memory>

namespace armajitto {

class Recompiler {
public:
    Recompiler(const Specification &spec);
    ~Recompiler();

    void Reset();

    TranslatorParameters &GetTranslatorParameters();
    OptimizerParameters &GetOptimizerParameters();

    CPUModel GetCPUModel() const;
    CPUArch GetCPUArch() const;

    arm::State &GetARMState();
    ISystem &GetSystem();

    const arm::State &GetARMState() const;
    const ISystem &GetSystem() const;

    uint64_t Run(uint64_t minCycles);

    void FlushCachedBlocks();
    void InvalidateCodeCache();
    void InvalidateCodeCacheRange(uint32_t start, uint32_t end);

private:
    Specification m_spec;
    Context m_context;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace armajitto
