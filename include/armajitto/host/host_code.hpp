#pragma once

#include "armajitto/core/context.hpp"

namespace armajitto {

struct HostCode {
    using Fn = void (*)(Context &context, arm::State &state);

    HostCode(Fn fn)
        : fn((fn == nullptr) ? NullFn : fn) {}

    void operator()(Context &context) {
        fn(context, context.GetARMState());
    }

private:
    Fn fn;

    static void NullFn(Context &, arm::State &) {
        // TODO: error handling
    }
};

} // namespace armajitto
