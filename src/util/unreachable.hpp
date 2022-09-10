#pragma once

namespace util {

[[noreturn]] inline void unreachable() {
#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
    __builtin_unreachable();
#elif defined(_MSC_VER)
    __assume(0);
#endif
}

} // namespace util
