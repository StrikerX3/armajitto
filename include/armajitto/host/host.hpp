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

    // Calls the compiled code at LocationRef, if present, and runs for the specified amount of cycles.
    // Returns the number of cycles executed (or skipped due to halting).
    // Returns 0 if there is no function at the specified location.
    virtual uint64_t Call(LocationRef loc, uint64_t cycles) = 0;

    // Calls the specified compiled code, if present.
    // Returns the number of cycles executed (or skipped due to halting).
    // Returns 0 if the code pointer is null.
    virtual uint64_t Call(HostCode code, uint64_t cycles) = 0;

    // Clears all compiled code.
    virtual void Clear() = 0;

protected:
    Context &m_context;
};

} // namespace armajitto
