#include "armajitto/core/allocator.hpp"

#include <cassert>

namespace armajitto::memory {

#ifdef _WIN32
static inline void *AlignedAlloc(std::size_t size, std::size_t alignment) {
    return _aligned_malloc(size, alignment);
}

static inline void AlignedFree(void *ptr) {
    _aligned_free(ptr);
}
#else
static inline void *AlignedAlloc(std::size_t size, std::size_t alignment) {
    return std::aligned_alloc(alignment, size);
}

static inline void AlignedFree(void *ptr) {
    std::free(ptr);
}
#endif

Allocator::Allocator() {
    AllocatePage(kPageChunkSize);
}

Allocator::~Allocator() {
    Release();
}

void *Allocator::AllocateRaw(std::size_t bytes, std::size_t alignment) {
    // Align size
    const std::size_t alignMask = alignment - 1;
    bytes = (bytes + alignMask) & ~alignMask;

    assert(m_head != nullptr);
    /*if (m_head == nullptr) {
        if (!AllocatePage(bytes)) {
            return nullptr;
        }
    }*/

    uint8_t *ptr = m_head->nextAlloc;
    auto alignOffset = ((uintptr_t(ptr) + alignMask) & ~alignMask) - uintptr_t(ptr) + sizeof(Page *);
    const auto freeSize = m_head->size - (m_head->nextAlloc - m_head->ptr);
    if (bytes + alignOffset > freeSize) {
        if (!AllocatePage(bytes)) {
            return nullptr;
        }
        ptr = m_head->nextAlloc;
        alignOffset = ((uintptr_t(ptr) + alignMask) & ~alignMask) - uintptr_t(ptr) + sizeof(Page *);
    }

    assert(ptr < m_head->ptr + m_head->size);
    assert(ptr + alignOffset + bytes <= m_head->ptr + m_head->size);

    *reinterpret_cast<Page **>(ptr) = m_head;
    m_head->nextAlloc += bytes + alignOffset;
    ++m_head->numAllocs;
    return ptr + alignOffset;
}

void Allocator::Free(void *p) {
    auto *page = *reinterpret_cast<Page **>(reinterpret_cast<uint8_t *>(p) - sizeof(Page *));
    ++page->numFrees;

    if (page->numAllocs == page->numFrees && page != m_head) {
        /*if (page->prev != nullptr) {
            page->prev->next = page->next;
        }*/
        page->prev->next = page->next;

        if (page->next != nullptr) {
            page->next->prev = page->prev;
        }

        /*if (m_head == page) {
            m_head = page->next;
        }*/

        AlignedFree(page->ptr);
        delete page;
    }
}

void Allocator::Release() {
    // Reset head page
    m_head->nextAlloc = m_head->ptr;
    m_head->numAllocs = 0;
    m_head->numFrees = 0;

    // Free all further pages
    Page *page = m_head->next;
    while (page != nullptr) {
        Page *next = page->next;
        AlignedFree(page->ptr);
        delete page;
        page = next;
    }
    m_head->next = nullptr;
}

bool Allocator::AllocatePage(std::size_t bytes) {
    if (bytes == 0) {
        bytes = kPageChunkSize;
    }
    std::size_t pageSize = (bytes + kPageChunkAlign) & ~kPageChunkAlign;
    void *ptr = AlignedAlloc(pageSize, kPageAlign);
    if (ptr == nullptr) {
        return false;
    }

    auto *page = new Page();
    page->ptr = static_cast<uint8_t *>(ptr);
    page->size = pageSize;
    page->nextAlloc = page->ptr;

    if (m_head != nullptr) {
        m_head->prev = page;
    }
    page->next = m_head;
    m_head = page;
    return true;
}

} // namespace armajitto::memory