#pragma once

#include <array>
#include <bit>
#include <limits>
#include <vector>

namespace armajitto::memory {

// TODO: customizable parameters
// TODO: buckets for different sizes of objects or alignments
class Allocator {
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

    ~Allocator();

    void *AllocateRaw(std::size_t bytes, std::size_t alignment = sizeof(void *));

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

    void Free(void *p);

    void Release();

private:
    static constexpr std::size_t kPageChunkSize = 1024u * 1024u; // 65536u;
    static constexpr std::size_t kPageChunkAlign = kPageChunkSize - 1;
    static constexpr std::size_t kPageAlign = 4096u;

    struct Page {
        uint8_t *ptr;
        std::size_t size;

        uint8_t *nextAlloc;
        std::size_t numAllocs = 0;
        std::size_t numFrees = 0;

        Page *prev = nullptr;
        Page *next = nullptr;
    };

    Page *m_head = nullptr;

    bool AllocatePage(std::size_t bytes);
};

} // namespace armajitto::memory
