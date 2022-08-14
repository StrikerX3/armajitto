#pragma once

#include <array>
#include <bitset>
#include <cassert>
#include <deque>
#include <memory>
#include <ranges>
#include <vector>

namespace armajitto::memory {

// Memory arena using chunks of fixed sizes, dynamically allocated as needed.
template <typename T>
class Arena final {
private:
    // Keeps track of free indices in an arena chunk.
    class FreeList final {
    public:
        FreeList(size_t size) { m_freeList.resize(size); }

        // Clears the free list.
        void Reset() {
            m_posRead = m_posWrite = 0;
            m_count = 0;
        }

        // Retrieves the next free index.
        // If there is a free index, returns true and sets index to the next free index.
        // Otherwise, returns false.
        bool Get(size_t &index) {
            if (m_count == 0) {
                return false;
            }
            index = m_freeList[m_posRead++];
            if (m_posRead == m_freeList.size()) {
                m_posRead = 0;
            }
            m_count--;
            return true;
        }

        // Adds a free index to the list. Assumes the user will never overflow the list.
        void Put(size_t index) {
            assert(m_count < m_freeList.size());
            m_freeList[m_posWrite++] = index;
            if (m_posWrite == m_freeList.size()) {
                m_posWrite = 0;
            }
            m_count++;
        }

        // Returns the number of entries in the free list.
        size_t Count() { return m_count; }

    private:
        std::vector<size_t> m_freeList;
        size_t m_posRead = 0;
        size_t m_posWrite = 0;
        size_t m_count = 0;
    };

    // A chunk of memory containing a fixed number of homogeneous objects of type T.
    class Chunk final {
    public:
        // Points to an entry in the chunk.
        // The entry acts like a smart pointer, automatically freeing the memory when it is destroyed.
        struct Entry final {
            Entry()
                : m_ptr(nullptr)
                , m_owner(nullptr) {}

            Entry(Entry &&entry)
                : m_ptr(entry.m_ptr)
                , m_owner(entry.m_owner)
                , m_token(entry.m_token) {
                entry.m_owner = nullptr; // Invalidate entry
            }

            ~Entry() { Release(); }

            Entry &operator=(Entry &&entry) {
                m_ptr = entry.m_ptr;
                m_owner = entry.m_owner;
                m_token = entry.m_token;
                entry.m_owner = nullptr; // Invalidate entry
                return *this;
            }

            // Retrieves the pointer to the underlying object.
            T *Get() { return (m_owner != nullptr && m_token == m_owner->m_token) ? m_ptr : nullptr; }
            T &operator*() { return *Get(); }
            T *operator->() { return Get(); }

            // Determines if the entry if valid.
            bool Valid() const { return (m_owner != nullptr) && (m_token == m_owner->m_token) && (m_ptr != nullptr); }
            operator bool() const { return Valid(); }

            void Release() {
                if (Valid()) {
                    m_owner->Free(m_ptr, m_token);
                    m_owner = nullptr;
                }
            }

        private:
            Entry(T *ptr, Chunk *owner, uintmax_t token)
                : m_ptr(ptr)
                , m_owner(owner)
                , m_token(token) {}

            T *m_ptr;
            Chunk *m_owner;

            // The arena chunk token that created this entry.
            // The entry is valid if this token matches the arena chunk's token.
            uintmax_t m_token;

            friend class Chunk;
        };

        Chunk(size_t size)
            : m_size(size)
            , m_freeList(size) {
            m_elems = new T[size];
            m_allocated.resize(size);
        }

        ~Chunk() { delete[] m_elems; }

        // Clears the chunk.
        void Reset() {
            for (size_t i = 0; i < m_size; i++) {
                m_allocated[i] = false;
            }
            m_freeList.Reset();
            m_next = 0;
            m_token++;
        }

        // Allocates a new entry if there is enough space available.
        Entry Allocate() { return {AllocateRaw(), this, m_token}; }

        // Determines how much space is available in the arena chunk, in number of entries.
        size_t Available() { return m_size - m_next + m_freeList.Count(); }

    private:
        // Returns a pointer to a free space in this chunk, or nullptr if there is no space.
        T *AllocateRaw() {
            size_t index = m_size;
            if (!m_freeList.Get(index)) {
                if (m_next < m_size) {
                    index = m_next++;
                }
            }
            if (index != m_size) {
                m_allocated[index] = true;
                return &m_elems[index];
            } else {
                return nullptr;
            }
        }

        // Frees the specified pointer if it belongs to this chunk and the token matches the current chunk's token.
        void Free(T *ptr, uintmax_t token) {
            if (token != m_token) {
                return;
            }
            size_t index = Find(ptr);
            if (index != m_size && m_allocated[index]) {
                m_allocated[index] = false;
                m_freeList.Put(index);
            }
        }

        // Retrieves the index of the specified pointer, if it belongs to this arena chunk.
        // Returns the size of the chunk if this chunk doesn't own the pointer.
        size_t Find(T *ptr) {
            if (ptr < &m_elems[0] || ptr > &m_elems[m_size - 1]) {
                return m_size;
            }
            return ptr - &m_elems[0];
        }

        size_t m_size;
        T *m_elems;
        std::vector<bool> m_allocated;
        FreeList m_freeList;
        size_t m_next = 0;

        // Chunk token, used to check that existing Chunk::Entry instances match this chunk's instance.
        // The token is incremented whenever Reset() is invoked, invalidating existing Chunk::Entry instances.
        uintmax_t m_token = 1;
    };

public:
    using ChunkEntry = typename Chunk::Entry;

    // Points to an entry in the arena.
    // The entry acts like a smart pointer, automatically freeing the memory when it is destroyed.
    struct Entry final {
        Entry()
            : m_owner(nullptr) {}

        Entry(ChunkEntry &&entry, size_t chunkIndex, Arena<T> *owner, uintmax_t token)
            : m_entry(std::move(entry))
            , m_chunkIndex(chunkIndex)
            , m_owner(owner)
            , m_token(token) {}

        Entry(Entry &&entry)
            : m_entry(std::move(entry.m_entry))
            , m_chunkIndex(entry.m_chunkIndex)
            , m_owner(entry.m_owner)
            , m_token(entry.m_token) {
            entry.m_owner = nullptr; // Invalidate entry
        }

        ~Entry() { Release(); }

        Entry &operator=(Entry &&entry) {
            m_entry = std::move(entry.m_entry);
            m_chunkIndex = entry.m_chunkIndex;
            m_owner = entry.m_owner;
            m_token = entry.m_token;
            entry.m_owner = nullptr; // Invalidate entry
            return *this;
        }

        // Retrieves the pointer to the underlying object.
        T *Get() { return (m_owner != nullptr && m_token == m_owner->m_token) ? m_entry.Get() : nullptr; }
        T &operator*() { return *Get(); }
        T *operator->() { return Get(); }

        // Determines if the entry if valid.
        bool Valid() const { return (m_owner != nullptr) && (m_token == m_owner->m_token) && m_entry.Valid(); }
        operator bool() const { return Valid(); }

        // Releases the entry if valid.
        void Release() {
            if (Valid()) {
                m_entry.Release();
                m_owner->Free(m_chunkIndex, m_token);
                m_owner = nullptr;
            }
        }

    private:
        ChunkEntry m_entry;
        size_t m_chunkIndex;
        Arena<T> *m_owner;
        uintmax_t m_token;

        friend class Arena<T>;
    };

    Arena(size_t chunkSize)
        : m_chunkSize(chunkSize) {}

    // Clears the arena, invalidating all existing entries.
    // If freeMemory is true, existing chunks will also be freed from memory.
    void Reset(bool freeMemory) {
        m_token++;
        if (freeMemory) {
            m_chunks.clear();
            m_openChunks.clear();
        } else {
            for (auto &chunk : m_chunks) {
                chunk->Reset();
            }
            m_openChunks.clear();
            for (size_t i = 0; i < m_chunks.size(); i++) {
                m_openChunks.push_back(i);
            }
        }
    }

    // Allocates a new entry.
    Entry Allocate() {
        auto entry = AllocateChunkEntry();
        return {std::move(entry.first), entry.second, this, m_token};
    }

private:
    // Size of each chunk.
    const size_t m_chunkSize;

    // Memory arena chunks.
    // We need to use smart pointers here as the arena chunks need to stay at a fixed position in memory.
    std::vector<std::unique_ptr<Chunk>> m_chunks;

    // List of chunks that have free space.
    std::deque<size_t> m_openChunks;

    // Arena token, used to check that existing Arena::Entry instances match this arena's instance.
    // The token is incremented whenever Reset() is invoked, invalidating existing Arena::Entry instances.
    uintmax_t m_token = 1;

    // Allocates a chunk entry, creating a new chunk if necessary.
    // Returns the newly created chunk and sets chunkIndex to the index of the chunk that owns the entry.
    std::pair<ChunkEntry, size_t> AllocateChunkEntry() {
        if (!m_openChunks.empty()) {
            // Get chunk from the open list
            auto chunkIndex = m_openChunks.front();
            auto &chunk = *m_chunks[chunkIndex];

            // Allocate entry
            auto entry = chunk.Allocate();

            // If the chunk is now full, remove it from the open list
            if (chunk.Available() == 0) {
                m_openChunks.pop_front();
            }
            return {std::move(entry), chunkIndex};
        } else {
            // Allocate new chunk
            auto chunkIndex = m_chunks.size();
            auto &chunk = *m_chunks.emplace_back(std::make_unique<Chunk>(m_chunkSize));

            // Allocate entry
            auto entry = chunk.Allocate();

            // If the chunk still has room, add it to the open list
            if (chunk.Available() > 0) {
                m_openChunks.push_back(chunkIndex);
            }
            return {std::move(entry), chunkIndex};
        }
    }

    // Marks the specified chunk as open if the token matches.
    void Free(size_t chunkIndex, uintmax_t token) {
        if (token != m_token) {
            return;
        }

        // Add chunk to the open list if it is currently full
        if (m_chunks[chunkIndex]->Available() == 0) {
            m_openChunks.push_back(chunkIndex);
        }
    }
};

} // namespace armajitto::memory
