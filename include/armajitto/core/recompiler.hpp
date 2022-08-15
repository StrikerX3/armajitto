#pragma once

#include "armajitto/defs/arm/state.hpp"
#include "context.hpp"
#include "specification.hpp"

#include <memory>

namespace armajitto {

class Recompiler {
public:
    Recompiler(const Specification &spec)
        : m_spec(spec)
        , m_context(spec.arch, spec.system) {}

    arm::State &GetARMState() {
        return m_context.GetARMState();
    }

    CPUArch GetCPUArch() const {
        return m_spec.arch;
    }

    ISystem &GetSystem() {
        return m_spec.system;
    }

    uint64_t Run(uint64_t minCycles);

private:
    Specification m_spec;
    Context m_context;
};

} // namespace armajitto
