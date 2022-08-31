#pragma once

#include "armajitto/core/context.hpp"
#include "armajitto/ir/basic_block.hpp"

namespace armajitto {

using HostCode = uintptr_t;

// Base class for host compilers and invokers.
class Host {
public:
    Host(Context &context)
        : m_context(context)
        , m_armState(context.GetARMState())
        , m_system(context.GetSystem()) {}

    virtual ~Host() = default;

    // Compiles the given basic block into callable host code and returns a pointer to the compiled code.
    // Use the block's LocationRef to call the code.
    virtual HostCode Compile(ir::BasicBlock &block) = 0;

    // Calls the compiled code at LocationRef, if present.
    virtual void Call(LocationRef loc) = 0;

    // Calls the specified compiled code, if present.
    virtual void Call(HostCode code) = 0;

protected:
    Context &m_context;
    arm::State &m_armState;
    ISystem &m_system;
};

} // namespace armajitto
