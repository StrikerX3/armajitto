#pragma once

#include <array>
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

    // Memory for the chunk structs is allocated in pages of this size.
    // Must be a multiple of the host page size.
    static constexpr std::size_t kChunkPageSize = 65536u;

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
                auto *desc = GetDescriptor(ptr);
                desc->allocator = this;
                return ptr;
            }
            prevChunk = chunk;
            chunk = chunk->next;
        }

        // No chunks have enough space to fulfill the requested allocation, so we need to create a new chunk

        // Allocate chunk from the current page
        chunk = AllocateChunk(bytes);
        if (chunk == nullptr) {
            return nullptr;
        }

        // Insert it at the head, since it is the most likely chunk to have free space
        chunk->next = m_head;
        m_head = chunk;

        // At this point, the chunk should have room for the requested allocation
        void *ptr = chunk->Allocate(bytes, alignment);
        auto *desc = GetDescriptor(ptr);
        desc->allocator = this;
        return ptr;
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
    Ref<T> Allocate(Args &&...args) {
        void *ptr = AllocateRaw(sizeof(T));
        if (ptr != nullptr) {
            return {*this, new (ptr) T(std::forward<Args>(args)...)};
        } else {
            return {*this, nullptr};
        }
    }

    void Free(void *p) {
        // Find the chunk that allocated this pointer and release the pointer
        auto *desc = GetDescriptor(p); // NOTE: might fail if the pointer was allocated elsewhere

        // Bail out if the pointer was not allocated by this allocator
        if (desc->allocator != this) {
            return;
        }

        Chunk *chunk = desc->chunk;
        if (chunk->Release(p)) {
            // This chunk owned p and has released it
            // If the chunk no longer has any allocations, free it
            if (chunk->IsEmpty()) {
                // TODO: speed this up somehow; maybe use doubly-linked list
                Chunk *prevChunk = m_head;
                while (prevChunk != nullptr) {
                    if (prevChunk->next == chunk) {
                        prevChunk->next = chunk->next;
                        break;
                    }
                }
                if (chunk == m_head) {
                    m_head = chunk->next;
                }
                FreeChunk(chunk);
            }
        }
    }

    void Release() {
        Chunk *chunk = m_head;
        while (chunk != nullptr) {
            Chunk *next = chunk->next;
            FreeChunk(chunk);
            chunk = next;
        }
        m_head = nullptr;
    }

private:
    struct ChunkPage;
    struct Chunk;

    // This is prepended to all allocated objects to speed up operations.
    struct AllocationDescriptor {
        Allocator *allocator;
        Chunk *chunk;
        void *ptr;
        std::size_t size;
    };

    static AllocationDescriptor *GetDescriptor(void *ptr) {
        return reinterpret_cast<AllocationDescriptor *>(reinterpret_cast<char *>(ptr) - sizeof(AllocationDescriptor));
    }

    struct Chunk {
        Chunk(void *basePtr, std::size_t size, ChunkPage *page)
            : basePtr(basePtr)
            , size(size)
            , page(page) {
            freeRegions.push_back({basePtr, size});
        }

        ~Chunk() {
            AlignedFree(basePtr);
        }

        void *Allocate(std::size_t bytes, std::size_t alignment) {
            const std::size_t alignMask = alignment - 1;
            constexpr std::size_t descSize = sizeof(AllocationDescriptor);

            // Align entry size
            bytes = (bytes + alignMask) & ~alignMask;

            auto setupDescriptor = [this, bytes](void *ptr) {
                auto *desc = reinterpret_cast<AllocationDescriptor *>(ptr);
                desc->chunk = this;
                desc->ptr = ptr;
                desc->size = bytes;
            };

            Region *bestFit = nullptr;
            for (auto it = freeRegions.begin(); it != freeRegions.end(); ++it) {
                auto &region = *it;
                const auto alignOffset =
                    ((uintptr_t(region.ptr) + alignMask) & ~alignMask) - uintptr_t(region.ptr) + descSize;
                const auto totalSize = bytes + alignOffset;
                if (region.size == totalSize) {
                    // Perfect fit; erase the region and return its pointer
                    void *ptr = region.ptr;
                    setupDescriptor(ptr);
                    freeRegions.erase(it);
                    return static_cast<char *>(ptr) + alignOffset;
                }
                if (region.size >= totalSize && (bestFit == nullptr || region.size < bestFit->size + alignOffset)) {
                    // Record best fit or use the first available region
                    bestFit = &region;
                }
            }
            if (bestFit != nullptr) {
                // Best fit found; resize and move region pointer
                char *ptr = static_cast<char *>(bestFit->ptr);
                setupDescriptor(ptr);

                const auto alignOffset = ((uintptr_t(ptr) + alignMask) & ~alignMask) - uintptr_t(ptr) + descSize;
                const auto totalSize = bytes + alignOffset;
                bestFit->ptr = ptr + totalSize;
                bestFit->size -= totalSize;
                return ptr + alignOffset;
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
            const auto *desc = GetDescriptor(ptr);
            if (desc->chunk != this) {
                return false;
            }
            const auto size = desc->size + sizeof(AllocationDescriptor);
            ptr = desc->ptr;

            // Merge free regions if possible
            auto itFree = Find(ptr, freeRegions);
            bool merged = false;
            if (itFree != freeRegions.end() && static_cast<char *>(ptr) + size == itFree->ptr) {
                // End of the freed region connects to the beginning of the existing free region
                itFree->ptr = ptr;
                itFree->size += size;
                merged = true;
            }
            if (itFree != freeRegions.begin() && itFree != freeRegions.end()) {
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
        ChunkPage *page;

    private:
        struct Region {
            void *ptr;
            std::size_t size;
        };

        std::vector<Region> freeRegions;

        void InsertSorted(void *ptr, std::size_t size, std::vector<Region> &vec) {
            auto it = Find(ptr, vec);
            vec.insert(it, {ptr, size});
        }

        typename std::vector<Region>::iterator Find(void *ptr, std::vector<Region> &vec) {
            return std::lower_bound(vec.begin(), vec.end(), ptr, [](Region &lhs, void *rhs) { return lhs.ptr < rhs; });
        }
    };

    // A chunk page contains multiple chunk descriptor slots which are handed out sequentially on chunk allocation
    // requests. Once all slots have been allocated and freed, the page is freed.
    struct ChunkPage {
        ChunkPage(ChunkPage *&currChunkPageRef)
            : m_currChunkPageRef(currChunkPageRef) {}

        Chunk *NextChunk() {
            if (m_usedChunks < kNumChunks) {
                return reinterpret_cast<Chunk *>(&m_chunks[(m_usedChunks++) * sizeof(Chunk)]);
            } else {
                return nullptr;
            }
        }

        bool FreeChunk() {
            m_freedChunks++;
            bool freed = (m_freedChunks == m_usedChunks) && (m_freedChunks == kNumChunks);
            if (freed && m_currChunkPageRef == this) {
                m_currChunkPageRef = nullptr;
            }
            return freed;
        }

    private:
        static constexpr auto kNumChunks = kChunkPageSize / sizeof(Chunk);
        alignas(kChunkMemAlignment) std::array<uint8_t, kNumChunks * sizeof(Chunk)> m_chunks;
        uint32_t m_usedChunks = 0;
        uint32_t m_freedChunks = 0;
        ChunkPage *&m_currChunkPageRef;
    };

    Chunk *AllocateChunk(std::size_t bytes) {
        // Chunk memory is allocated in kChunkSize units
        const std::size_t chunkSize = (bytes + kChunkMemSize - 1) & ~(kChunkMemSize - 1);
        void *basePtr = AlignedAlloc(chunkSize, kChunkMemAlignment);
        if (basePtr == nullptr) {
            // Failed to allocate memory for the chunk data
            return nullptr;
        }

        // Allocate chunk from the current page if possible
        Chunk *chunk = m_currChunkPage->NextChunk();
        if (chunk == nullptr) {
            // Not enough space in the current page; allocate new page
            m_currChunkPage = new ChunkPage(m_currChunkPage);
            if (m_currChunkPage == nullptr) {
                // Failed to allocate memory for the chunk page
                AlignedFree(basePtr);
                return nullptr;
            }
            chunk = m_currChunkPage->NextChunk();
        }

        // Initialize chunk
        new (chunk) Chunk(basePtr, chunkSize, m_currChunkPage);
        return chunk;
    }

    static void FreeChunk(Chunk *chunk) {
        ChunkPage *page = chunk->page;
        chunk->~Chunk();
        if (page->FreeChunk()) {
            delete page;
        }
    }

    Chunk *m_head = nullptr;
    ChunkPage *m_currChunkPage = new ChunkPage(m_currChunkPage);

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
