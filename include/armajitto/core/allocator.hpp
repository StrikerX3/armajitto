#pragma once

#include <algorithm>
#include <cstdlib>
#include <type_traits>
#include <utility>
#include <vector>

namespace armajitto::memory {

class Allocator {
    static constexpr std::size_t pageAlignmentShift = 12;
    static constexpr std::size_t alignment = static_cast<std::size_t>(1) << pageAlignmentShift;

    static constexpr std::size_t pageShift = 16;
    static constexpr std::size_t pageSize = static_cast<std::size_t>(1) << pageShift;
    static constexpr std::size_t pageMask = pageSize - 1;

    static constexpr std::size_t entryAlignmentShift = 4;
    static constexpr std::size_t entryAlignmentSize = static_cast<std::size_t>(1) << entryAlignmentShift;
    static constexpr std::size_t entryAlignmentMask = entryAlignmentSize - 1;

public:
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
        for (auto &block : m_blocks) {
            AlignedFree(block.basePtr);
        }
    }

    template <typename T, typename... Args, typename = std::enable_if_t<std::is_trivially_destructible_v<T>>>
    T *Allocate(Args &&...args) {
        void *ptr = AllocateMemory(sizeof(T));
        if (ptr != nullptr) {
            return new (ptr) T(std::forward<Args>(args)...);
        } else {
            return nullptr;
        }
    }

    template <typename T, typename... Args, typename = std::enable_if_t<!std::is_trivially_destructible_v<T>>>
    Ref<T> AllocateNonTrivial(Args &&...args) {
        void *ptr = AllocateMemory(sizeof(T));
        if (ptr != nullptr) {
            return {*this, new (ptr) T(std::forward<Args>(args)...)};
        } else {
            return {*this, nullptr};
        }
    }

    void Free(void *ptr) {
        for (auto it = m_blocks.begin(); it != m_blocks.end(); ++it) {
            if (it->Free(ptr)) {
                if (it->IsEmpty()) {
                    AlignedFree(it->basePtr);
                    m_blocks.erase(it);
                }
                break;
            }
        }
    }

private:
    void *AllocateMemory(std::size_t size) {
        for (auto it = m_blocks.begin(); it != m_blocks.end(); ++it) {
            void *ptr = it->Allocate(size);
            if (ptr != nullptr) {
                // Move this block to the front to speed up future allocations
                if (it != m_blocks.begin()) {
                    std::iter_swap(it, m_blocks.begin());
                }
                return ptr;
            }
        }
        // No blocks had enough space for this object, so allocate a new block with the specified page size
        auto *block = AllocateBlock(size);
        if (block != nullptr) {
            return block->Allocate(size);
        } else {
            return nullptr;
        }
    }

    struct Block {
        Block(void *ptr, std::size_t size)
            : basePtr(ptr)
            , endPtr(static_cast<char *>(ptr) + size)
            , blockSize(size) {
            freeRegions.push_back({ptr, size});
        }

        void *Allocate(std::size_t size) {
            // Align entry size
            size = (size + entryAlignmentSize - 1) & ~entryAlignmentMask;

            Region *bestFit = nullptr;
            for (auto it = freeRegions.begin(); it != freeRegions.end(); ++it) {
                auto &region = *it;
                if (region.size < size) {
                    // Too small
                    continue;
                }
                if (region.size == size) {
                    // Perfect fit; erase the region and return its pointer
                    void *ptr = region.ptr;
                    freeRegions.erase(it);
                    InsertSorted(ptr, size, allocRegions);
                    return ptr;
                }
                if (bestFit == nullptr || region.size < bestFit->size) {
                    // Record best fit or use the first available region
                    bestFit = &region;
                }
            }
            if (bestFit != nullptr) {
                // Best fit found; resize and move region pointer
                void *ptr = bestFit->ptr;
                bestFit->ptr = static_cast<char *>(bestFit->ptr) + size;
                bestFit->size -= size;
                InsertSorted(ptr, size, allocRegions);
                return ptr;
            }
            // No free region found with the requested size
            return nullptr;
        }

        bool Free(void *ptr) {
            // Ensure the pointer belongs to this block
            if (ptr < basePtr || ptr >= endPtr) {
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

            // Coalesce free regions if possible
            auto itFree = Find(ptr, freeRegions);
            bool coalesced = false;
            if (itFree != freeRegions.end() && static_cast<char *>(ptr) + size == itFree->ptr) {
                // End of the freed region connects to the beginning of the existing free region
                itFree->ptr = ptr;
                itFree->size += size;
                coalesced = true;
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
                    coalesced = true;
                }
            }
            if (!coalesced) {
                // Add new free region
                InsertSorted(ptr, size, freeRegions);
            }

            // Indicate success
            return true;
        }

        bool IsEmpty() const {
            return (freeRegions.size() == 1 && freeRegions[0].size == blockSize);
        }

        struct Region {
            void *ptr;
            std::size_t size;
        };

        void *basePtr;
        void *endPtr;
        std::size_t blockSize;

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

    Block *AllocateBlock(std::size_t sizeBytes) {
        if (sizeBytes == 0) {
            return nullptr;
        }

        // Align to page size
        sizeBytes = (sizeBytes + pageSize - 1) & ~pageMask;
        void *ptr = AlignedAlloc(sizeBytes);
        if (ptr != nullptr) {
            return &m_blocks.emplace_back(ptr, sizeBytes);
        } else {
            return nullptr;
        }
    }

    std::vector<Block> m_blocks;

#ifdef _WIN32
    static void *AlignedAlloc(std::size_t size) {
        return _aligned_malloc(size, alignment);
    }

    static void AlignedFree(void *ptr) {
        _aligned_free(ptr);
    }
#else
    static void *AlignedAlloc(std::size_t size) {
        return std::aligned_alloc(alignment, size);
    }

    static void AlignedFree(void *ptr) {
        std::free(ptr);
    }
#endif
};

} // namespace armajitto::memory
