#pragma once

namespace util {

template <typename Fn>
struct ScopeGuard {
    ScopeGuard(Fn &&fn)
        : fn(std::move(fn)) {}

    ~ScopeGuard() {
        if (!cancelled) {
            fn();
        }
    }

    void Cancel() {
        cancelled = true;
    }

private:
    Fn fn;
    bool cancelled = false;
};

} // namespace util
