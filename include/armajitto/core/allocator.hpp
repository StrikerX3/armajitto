#pragma once

#include <bit>
#include <limits>
#include <vector>

namespace armajitto::memory {

class Allocator {
public:
    // Memory allocated for chunks is a multiple of this amount.
    // Must be a power of two.
    static constexpr std::size_t kChunkMemSize = 65536u;

    // Memory allocated for chunks is aligned to this size.
    // Must be a power of two.
    static constexpr std::size_t kChunkMemAlignment = 4096u;

    // TODO: allocate and free Chunks in page-sized sets using this constant
    // Memory for the chunk structs is allocated in pages of this size.
    // Must be a power of two and no smaller than the host page size.
    static constexpr std::size_t kChunkPageSize = 4096u;

    // Reference to a non-trivially-destructible object allocated by this allocator.
    // Invokes the object's destructor upon destruction.
    template <typename T>
    struct Ref {
        Ref(const Ref &) = delete;
        Ref(Ref &&obj)
            : allocator(obj.allocator) {
            std::swap(ptr, obj.ptr);
        }
        ~Ref() {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                if (ptr != nullptr) {
                    ptr->~T();
                }
            }
            allocator.Free(ptr);
        }

        T *operator&() {
            return ptr;
        }

        const T *operator&() const {
            return ptr;
        }

        T &operator*() {
            return *ptr;
        }

        const T &operator*() const {
            return *ptr;
        }

        T *operator->() {
            return ptr;
        }

        const T *operator->() const {
            return ptr;
        }

        bool IsValid() const {
            return ptr != nullptr;
        }

        operator bool() const {
            return IsValid();
        }

    private:
        Ref(Allocator &allocator, T *ptr)
            : allocator(allocator)
            , ptr(ptr) {}

        Allocator &allocator;
        T *ptr = nullptr;

        friend class Allocator;
    };

    ~Allocator() {
        Release();
    }

    void *AllocateRaw(std::size_t bytes, std::size_t alignment = sizeof(void *)) {
        // Traverse the chunk chain trying to perform the requested allocation
        Chunk *chunk = m_head;
        Chunk *prevChunk = nullptr;
        while (chunk != nullptr) {
            void *ptr = chunk->Allocate(bytes, alignment);
            if (ptr != nullptr) {
                // This chunk allocated the block successfully
                if (chunk != m_head) {
                    // Move it to the head since it is likely to have free space for future allocations
                    if (prevChunk != nullptr) {
                        prevChunk->next = chunk->next;
                    }
                    chunk->next = m_head;
                    m_head = chunk;
                }
                return ptr;
            }
            prevChunk = chunk;
            chunk = chunk->next;
        }

        // No chunks have enough space to fulfill the requested allocation, so we need to create a new chunk

        // Chunk memory is allocated in kChunkSize units
        const std::size_t chunkSize = (bytes + kChunkMemSize - 1) & ~(kChunkMemSize - 1);
        void *basePtr = AlignedAlloc(chunkSize, kChunkMemAlignment);
        if (basePtr == nullptr) {
            // Failed to allocate memory for the chunk data
            return nullptr;
        }

        chunk = new Chunk(basePtr, chunkSize);
        if (chunk == nullptr) {
            // Failed to allocate memory for the Chunk object
            AlignedFree(basePtr);
            return nullptr;
        }

        // Insert it at the head, since it is the most likely chunk to have free space
        chunk->next = m_head;
        m_head = chunk;

        // At this point, the chunk should have room for the requested allocation
        return chunk->Allocate(bytes, alignment);
    }

    template <typename T, typename... Args, typename = std::enable_if_t<std::is_trivially_destructible_v<T>>>
    T *Allocate(Args &&...args) {
        void *ptr = AllocateRaw(sizeof(T));
        if (ptr != nullptr) {
            return new (ptr) T(std::forward<Args>(args)...);
        } else {
            return nullptr;
        }
    }

    template <typename T, typename... Args, typename = std::enable_if_t<!std::is_trivially_destructible_v<T>>>
    Ref<T> AllocateNonTrivial(Args &&...args) {
        void *ptr = AllocateRaw(sizeof(T));
        if (ptr != nullptr) {
            return {*this, new (ptr) T(std::forward<Args>(args)...)};
        } else {
            return {*this, nullptr};
        }
    }

    void Free(void *p) {
        // Find the chunk that allocated this pointer and release the pointer
        // TODO: speed up pointer -> chunk lookups somehow
        Chunk *chunk = m_head;
        Chunk *prevChunk = nullptr;
        while (chunk != nullptr) {
            if (chunk->Release(p)) {
                // This chunk owned p and has released it
                // If the chunk no longer has any allocations, free it
                if (chunk->IsEmpty()) {
                    if (prevChunk != nullptr) {
                        prevChunk->next = chunk->next;
                    }
                    if (chunk == m_head) {
                        m_head = nullptr;
                    }
                    delete chunk;
                }
                break;
            }
            prevChunk = chunk;
            chunk = chunk->next;
        }
    }

    void Release() {
        Chunk *chunk = m_head;
        while (chunk != nullptr) {
            Chunk *next = chunk->next;
            delete chunk;
            chunk = next;
        }
        m_head = nullptr;
    }

private:
    struct Chunk {
        Chunk(void *basePtr, std::size_t size)
            : basePtr(basePtr)
            , size(size) {
            freeRegions.push_back({basePtr, size});
        }

        ~Chunk() {
            AlignedFree(basePtr);
        }

        void *Allocate(std::size_t bytes, std::size_t alignment) {
            const std::size_t alignMask = alignment - 1;

            // Align entry size
            bytes = (bytes + alignMask) & ~alignMask;

            Region *bestFit = nullptr;
            for (auto it = freeRegions.begin(); it != freeRegions.end(); ++it) {
                auto &region = *it;
                const auto alignOffset = ((uintptr_t(region.ptr) + alignMask) & ~alignMask) - uintptr_t(region.ptr);
                if (region.size < bytes + alignOffset) {
                    // Too small
                    continue;
                }
                if (region.size == bytes + alignOffset) {
                    // Perfect fit; erase the region and return its pointer
                    void *ptr = region.ptr;
                    freeRegions.erase(it);
                    InsertSorted(ptr, bytes, allocRegions);
                    return static_cast<char *>(ptr) + alignOffset;
                }
                if (bestFit == nullptr || region.size < bestFit->size + alignOffset) {
                    // Record best fit or use the first available region
                    bestFit = &region;
                }
            }
            if (bestFit != nullptr) {
                // Best fit found; resize and move region pointer
                char *ptr = static_cast<char *>(bestFit->ptr);
                const auto alignOffset = ((uintptr_t(ptr) + alignMask) & ~alignMask) - uintptr_t(ptr);
                bestFit->ptr = ptr + bytes + alignOffset;
                bestFit->size -= bytes + alignOffset;
                ptr += alignOffset;
                InsertSorted(ptr, bytes, allocRegions);
                return ptr;
            }

            // No free region found with the requested size
            return nullptr;
        }

        bool Release(void *ptr) {
            // Ensure the pointer belongs to this block
            if (ptr < basePtr || ptr >= static_cast<char *>(basePtr) + size) {
                return false;
            }

            // Remove allocated region
            auto itAlloc = Find(ptr, allocRegions);
            if (itAlloc == allocRegions.end()) {
                return false;
            }
            if (itAlloc->ptr != ptr) {
                return false;
            }
            auto size = itAlloc->size;
            allocRegions.erase(itAlloc);

            // Merge free regions if possible
            auto itFree = Find(ptr, freeRegions);
            bool merged = false;
            if (itFree != freeRegions.end() && static_cast<char *>(ptr) + size == itFree->ptr) {
                // End of the freed region connects to the beginning of the existing free region
                itFree->ptr = ptr;
                itFree->size += size;
                merged = true;
            }
            if (itFree != freeRegions.begin()) {
                auto itPrevFree = std::prev(itFree);
                if (static_cast<char *>(itPrevFree->ptr) + itPrevFree->size == ptr) {
                    // Beginning of freed region connects to the end of the previous free region
                    if (static_cast<char *>(itPrevFree->ptr) + itPrevFree->size == itFree->ptr) {
                        // Merge previous, freed and next regions together
                        itPrevFree->size += itFree->size;
                        freeRegions.erase(itFree);
                    } else {
                        // Merge freed region with previous region
                        itPrevFree->size += size;
                    }
                    merged = true;
                }
            }
            if (!merged) {
                // Add new free region
                InsertSorted(ptr, size, freeRegions);
            }

            // Indicate success
            return true;
        }

        bool IsEmpty() const {
            return (freeRegions.size() == 1 && freeRegions[0].size == size);
        }

        void *basePtr;
        std::size_t size;

        Chunk *next = nullptr;

    private:
        struct Region {
            void *ptr;
            std::size_t size;
        };

        std::vector<Region> freeRegions;
        std::vector<Region> allocRegions;

        void InsertSorted(void *ptr, std::size_t size, std::vector<Region> &vec) {
            auto it = Find(ptr, vec);
            vec.insert(it, {ptr, size});
        }

        typename std::vector<Region>::iterator Find(void *ptr, std::vector<Region> &vec) {
            return std::lower_bound(vec.begin(), vec.end(), ptr, [](Region &lhs, void *rhs) { return lhs.ptr < rhs; });
        }
    };

    Chunk *m_head = nullptr;

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
};

} // namespace armajitto::memory
