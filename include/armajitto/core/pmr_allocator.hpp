#pragma once

#include "allocator.hpp"

#include <memory_resource>

namespace armajitto::memory {

struct PMRAllocatorWrapper final : public std::pmr::memory_resource {
    PMRAllocatorWrapper(Allocator &allocator)
        : m_allocator(allocator) {}

    void *do_allocate(std::size_t bytes, std::size_t alignment) final {
        return m_allocator.AllocateRaw(bytes, alignment);
    }

    void do_deallocate(void *p, std::size_t /*bytes*/, std::size_t /*alignment*/) final {
        m_allocator.Free(p);
    }

    bool do_is_equal(const std::pmr::memory_resource &other) const noexcept final {
        return this == &other;
    }

private:
    Allocator &m_allocator;
};

} // namespace armajitto::memory
