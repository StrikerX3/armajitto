#pragma once

#include "core/allocator.hpp"
#include "util/pointer_cast.hpp"

#include "host_code.hpp"

#include <algorithm>
#include <cstdint>

namespace armajitto {

class BlockCache final {
public:
    static constexpr auto kValueSize = sizeof(HostCode);

    static constexpr uint64_t kL1Bits = 13;
    static constexpr uint64_t kL2Bits = 13;
    static constexpr uint64_t kL3Bits = 12;
    static constexpr uint64_t kKeyBits = kL1Bits + kL2Bits + kL3Bits;

    static constexpr uint64_t kL1Size = 1u << kL1Bits;
    static constexpr uint64_t kL1Mask = kL1Size - 1u;
    static constexpr uint64_t kL1Shift = kL2Bits + kL3Bits;

    static constexpr uint64_t kL2Size = 1u << kL2Bits;
    static constexpr uint64_t kL2Mask = kL2Size - 1u;
    static constexpr uint64_t kL2Shift = kL3Bits;

    static constexpr uint64_t kL3Size = 1u << kL3Bits;
    static constexpr uint64_t kL3Mask = kL3Size - 1u;
    static constexpr uint64_t kL3Shift = 0;

    BlockCache() {
        m_map = (Page *)m_allocator.AllocateRaw(sizeof(Page) * kL1Size, 16);
        std::fill_n(m_map, kL1Size, nullptr);
    }

    HostCode *Get(uint64_t key) const {
        const auto l1Index = Level1Index(key);
        if (m_map[l1Index] == nullptr) {
            return nullptr;
        }

        const auto l2Index = Level2Index(key);
        if (m_map[l1Index][l2Index] == nullptr) {
            return nullptr;
        }

        const auto l3Index = Level3Index(key);
        return &m_map[l1Index][l2Index][l3Index];
    }

    HostCode &GetOrCreate(uint64_t key) {
        const auto l1Index = Level1Index(key);
        if (m_map[l1Index] == nullptr) {
            m_map[l1Index] = (Block *)m_allocator.AllocateRaw(sizeof(Block) * kL2Size);
            std::fill_n(m_map[l1Index], kL2Size, nullptr);
        }

        const auto l2Index = Level2Index(key);
        if (m_map[l1Index][l2Index] == nullptr) {
            m_map[l1Index][l2Index] = (HostCode *)m_allocator.AllocateRaw(sizeof(HostCode) * kL3Size);
            std::fill_n(m_map[l1Index][l2Index], kL3Size, nullptr);
        }

        const auto l3Index = Level3Index(key);
        return m_map[l1Index][l2Index][l3Index];
    }

    void Clear() {
        m_allocator.Release();
        m_map = (Page *)m_allocator.AllocateRaw(sizeof(Page) * kL1Size, 16);
        std::fill_n(m_map, kL1Size, nullptr);
    }

    uintptr_t MapAddress() const {
        return CastUintPtr(m_map);
    }

private:
    memory::Allocator m_allocator;

    using Block = HostCode *; // array of kL3Size HostCodes
    using Page = Block *;     // array of kL2Size Blocks
    Page *m_map = nullptr;    // array of kL1Size Pages

    static constexpr uint64_t Level1Index(uint64_t key) {
        return (key >> kL1Shift) & kL1Mask;
    }

    static constexpr uint64_t Level2Index(uint64_t key) {
        return (key >> kL2Shift) & kL2Mask;
    }

    static constexpr uint64_t Level3Index(uint64_t key) {
        return (key >> kL3Shift) & kL3Mask;
    }
};

} // namespace armajitto
