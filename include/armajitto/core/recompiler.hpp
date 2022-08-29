#pragma once

#include "armajitto/guest/arm/state.hpp"
#include "context.hpp"
#include "specification.hpp"

#include <memory>

namespace armajitto {

class Recompiler {
public:
    Recompiler(const Specification &spec)
        : m_spec(spec)
        , m_context(spec.model, spec.system) {}

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

    uint64_t Run(uint64_t minCycles);

private:
    Specification m_spec;
    Context m_context;
};

} // namespace armajitto
