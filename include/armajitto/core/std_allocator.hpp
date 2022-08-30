#pragma once

#include "allocator.hpp"

namespace armajitto::memory {

struct StdAllocatorBase {
    static Allocator s_allocator;
};

inline Allocator StdAllocatorBase::s_allocator{};

// -----------------------------------------------------------------------------

template <typename T>
class StdAllocator : StdAllocatorBase {
public:
    using value_type = T;

    StdAllocator() = default;

    template <typename U>
    StdAllocator(const StdAllocator<U> &) {}

    T *allocate(std::size_t n) {
        return static_cast<T *>(s_allocator.AllocateRaw(n * sizeof(T)));
    }

    void deallocate(T *ptr, size_t) {
        s_allocator.Free(ptr);
    }

    bool operator==(const StdAllocator<T> &) const noexcept {
        return true;
    }

    bool operator!=(const StdAllocator<T> &) const noexcept {
        return false;
    }
};

} // namespace armajitto::memory
