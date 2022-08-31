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

    // Compiles the given basic block into callable host code and returns a pointer to the compiled code.
    // Use the block's LocationRef to call the code.
    virtual HostCode Compile(ir::BasicBlock &block) = 0;

    // Retrieves the compiled code for the specified location, if present.
    // Returns 0 if no code was compiled at that location.
    virtual HostCode GetCodeForLocation(LocationRef loc) = 0;

    // Calls the compiled code at LocationRef, if present.
    virtual void Call(LocationRef loc) = 0;

    // Calls the specified compiled code, if present.
    virtual void Call(HostCode code) = 0;

    // Clears all compiled code.
    virtual void Clear() = 0;

protected:
    Context &m_context;
};

} // namespace armajitto
