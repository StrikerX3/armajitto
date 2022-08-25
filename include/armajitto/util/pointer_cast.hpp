#pragma once

#include <cstdint>

// Cast any pointer to uintptr_t in a non-verbose manner.
inline uintptr_t CastUintPtr(void *ptr) {
    return reinterpret_cast<uintptr_t>(ptr);
}
