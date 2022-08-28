#pragma once

#include "armajitto/core/context.hpp"
#include "armajitto/ir/basic_block.hpp"
#include "host_code.hpp"

namespace armajitto {

// Base class for host compilers and invokers.
class Host {
public:
    Host(Context &context)
        : m_context(context)
        , m_armState(context.GetARMState())
        , m_system(context.GetSystem()) {}

    virtual ~Host() = default;

    // Compiles the given basic block into callable host code.
    virtual void Compile(ir::BasicBlock &block) = 0;

    // Calls the compiled code.
    virtual void Call(const ir::BasicBlock &block) = 0;

protected:
    Context &m_context;
    arm::State &m_armState;
    ISystem &m_system;

    // Helper method that gives implementors access to BasicBlock::GetHostCode().
    HostCode GetHostCode(const ir::BasicBlock &block) const {
        return block.GetHostCode();
    }

    // Helper method that gives implementors access to BasicBlock::SetHostCode(HostCode).
    void SetHostCode(ir::BasicBlock &block, HostCode code) {
        block.SetHostCode(code);
    }
};

} // namespace armajitto
