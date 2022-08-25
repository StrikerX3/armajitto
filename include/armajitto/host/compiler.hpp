#pragma once

#include "armajitto/core/context.hpp"
#include "armajitto/ir/basic_block.hpp"
#include "host_code.hpp"

namespace armajitto {

// Base class for host code compilers.
class Compiler {
public:
    Compiler(Context &context)
        : m_context(context) {}

    virtual ~Compiler() = default;

    // Compiles the given basic block into callable host code.
    virtual HostCode Compile(const ir::BasicBlock &block) = 0;

protected:
    Context &m_context;
};

} // namespace armajitto
