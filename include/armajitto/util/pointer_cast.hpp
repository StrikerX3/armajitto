#pragma once

#include <cstdint>

// Cast any pointer to uintptr_t in a non-verbose manner.
template <typename T>
inline uintptr_t CastUintPtr(T *ptr) {
    return reinterpret_cast<uintptr_t>(reinterpret_cast<void *>(ptr));
}
