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

} // namespace armajitto
