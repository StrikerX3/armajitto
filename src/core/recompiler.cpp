#include "armajitto/core/recompiler.hpp"

#include "context.hpp"

namespace armajitto {

Recompiler::Recompiler(const Specification &spec)
    : m_spec(spec)
    , m_context(std::make_unique<Context>(spec.arch, spec.system)) {}

Recompiler::~Recompiler() {}

arm::State &Recompiler::GetARMState() {
    return m_context->GetARMState();
}

uint64_t Recompiler::Run(uint64_t minCycles) {
    uint64_t cyclesExecuted = 0;
    while (cyclesExecuted < minCycles) {
        // TODO: do the JIT magic here
        ++cyclesExecuted;
    }
    return cyclesExecuted;
}

} // namespace armajitto
