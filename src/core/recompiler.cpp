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
        // TODO: implement IR emitter as the client for the decoder
        // - block should contain an emitter

        // TODO: do the JIT magic here
        // - find cached block
        // - if found, execute
        // - otherwise, generate block

        ++cyclesExecuted;
    }
    return cyclesExecuted;
}

} // namespace armajitto
