#pragma once

#include "armajitto/util/pointer_cast.hpp"

namespace armajitto {

struct HostCode {
    using Fn = void (*)();

    HostCode()
        : fn(NullFn) {}

    HostCode(Fn fn) {
        operator=(fn);
    }

    HostCode &operator=(Fn fn) {
        this->fn = (fn == nullptr) ? NullFn : fn;
        return *this;
    }

    uintptr_t GetPtr() const {
        return CastUintPtr(fn);
    }

private:
    Fn fn;

    static void NullFn() {
        // TODO: error handling
    }
};

} // namespace armajitto
