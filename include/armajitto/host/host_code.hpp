#pragma once

namespace armajitto {

struct HostCode {
    using Fn = void (*)();

    HostCode(Fn fn)
        : fn((fn == nullptr) ? NullFn : fn) {}

    void operator()() {
        fn();
    }

private:
    Fn fn;

    static void NullFn() {
        // TODO: error handling
    }
};

} // namespace armajitto
