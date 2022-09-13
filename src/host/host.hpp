#pragma once

#include "armajitto/core/context.hpp"
#include "armajitto/core/options.hpp"

#include "ir/basic_block.hpp"

#include "guest/arm/coprocessors/cp15_host_access.hpp"
#include "guest/arm/state_offsets.hpp"

#include "host_code.hpp"

namespace armajitto {

// Base class for host compilers and invokers.
class Host {
public:
    Host(Context &context, Options::Compiler &options)
        : m_context(context)
        , m_options(options)
        , m_stateOffsets(context.GetARMState()) {}

    virtual ~Host() = default;

    // Compiles the given basic block into callable host code and returns a pointer to the compiled code.
    // Use the block's LocationRef to call the code.
    virtual HostCode Compile(ir::BasicBlock &block) = 0;

    // Retrieves the compiled code for the specified location, if present.
    // Returns 0 if no code was compiled at that location.
    virtual HostCode GetCodeForLocation(LocationRef loc) = 0;

    // Calls the compiled code at LocationRef, if present, and runs for the specified amount of cycles.
    // Returns the number of cycles remaining after execution.
    // If negative, the call executed more cycles than requested.
    // If positive, the call executed less cycles than requested.
    // If zero, the call executed for exactly the requested amount of cycles.
    // Returns <cycles> if there is no function at the specified location.
    virtual int64_t Call(LocationRef loc, uint64_t cycles) = 0;

    // Calls the specified compiled code, if present.
    // Returns the number of cycles remaining after execution.
    // If negative, the call executed more cycles than requested.
    // If positive, the call executed less cycles than requested.
    // If zero, the call executed for exactly the requested amount of cycles.
    // Returns <cycles> if there is no function at the specified location.
    virtual int64_t Call(HostCode code, uint64_t cycles) = 0;

    // Clears all compiled code.
    virtual void Clear() = 0;

    // Invalidates all cached code blocks.
    virtual void InvalidateCodeCache() = 0;

    // Invalidates all cached code blocks in the specified range. Both <start> and <end> are inclusive.
    virtual void InvalidateCodeCacheRange(uint32_t start, uint32_t end) = 0;

protected:
    Context &m_context;
    Options::Compiler &m_options;

    arm::StateOffsets m_stateOffsets;

    void SetInvalidateCodeCacheCallback(arm::InvalidateCodeCacheCallback callback, void *ctx) {
        arm::SystemControlCoprocessor::HostAccess{m_context.GetARMState().GetSystemControlCoprocessor()}
            .SetInvalidateCodeCacheCallback(callback, ctx);
    }
};

} // namespace armajitto
