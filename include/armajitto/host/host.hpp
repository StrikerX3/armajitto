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
    virtual void Compile(ir::BasicBlock &block) = 0;

    // Calls the compiled code.
    virtual void Call(const ir::BasicBlock &block) = 0;

protected:
    Context &m_context;

    HostCode GetHostCode(const ir::BasicBlock &block) const {
        return block.GetHostCode();
    }

    void SetHostCode(ir::BasicBlock &block, HostCode code) {
        block.SetHostCode(code);
    }
};

} // namespace armajitto
