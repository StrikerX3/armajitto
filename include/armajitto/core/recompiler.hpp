#pragma once

#include "armajitto/arm/state.hpp"
#include "specification.hpp"

#include <memory>

namespace armajitto {

class Context;

class Recompiler {
public:
    Recompiler(const Specification &spec);
    ~Recompiler();

    arm::State &GetARMState();

    CPUArch GetCPUArch() const {
        return m_spec.arch;
    }

    ISystem &GetSystem() {
        return m_spec.system;
    }

    uint64_t Run(uint64_t minCycles);

private:
    Specification m_spec;
    std::unique_ptr<Context> m_context;
};

} // namespace armajitto
