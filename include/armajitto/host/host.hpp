#pragma once

#include "armajitto/core/context.hpp"
#include "armajitto/ir/basic_block.hpp"
#include "host_code.hpp"

namespace armajitto {

// Base class for host compilers and invokers.
class Host {
public:
    Host(Context &context)
        : m_context(context) {}

    virtual ~Host() = default;

    // Compiles the given basic block into callable host code.
    // TODO: move HostCode into BasicBlock
    virtual HostCode Compile(const ir::BasicBlock &block) = 0;

    // Calls the compiled code.
    // TODO: pass in BasicBlock as argument
    virtual void Call(HostCode code) = 0;

protected:
    Context &m_context;
};

} // namespace armajitto
