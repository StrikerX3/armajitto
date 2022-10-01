#pragma once

#include "core/allocator.hpp"
#include "pointer_cast.hpp"

#include <array>
#include <concepts>
#include <cstdint>
#include <memory>
#include <utility>

namespace util {

template <std::integral TKey, typename TValue, TKey level1Bits, TKey level2Bits, TKey level3Bits>
class ThreeLevelArray final {
    static constexpr TKey kOne = static_cast<TKey>(1);

public:
    static constexpr auto kValueSize = sizeof(TValue);

    static constexpr TKey kKeyBits = level1Bits + level2Bits + level3Bits;

    static constexpr TKey kL1Bits = level1Bits;
    static constexpr TKey kL1Size = kOne << kL1Bits;
    static constexpr TKey kL1Mask = kL1Size - kOne;
    static constexpr TKey kL1Shift = level2Bits + level3Bits;

    static constexpr TKey kL2Bits = level2Bits;
    static constexpr TKey kL2Size = kOne << kL2Bits;
    static constexpr TKey kL2Mask = kL2Size - kOne;
    static constexpr TKey kL2Shift = level3Bits;

    static constexpr TKey kL3Bits = level3Bits;
    static constexpr TKey kL3Size = kOne << kL3Bits;
    static constexpr TKey kL3Mask = kL3Size - kOne;
    static constexpr TKey kL3Shift = 0;

    ThreeLevelArray() {
        m_map = (Page *)m_allocator.AllocateRaw(sizeof(Page) * kL1Size, 16);
        std::fill_n(m_map, kL1Size, nullptr);
    }

    TValue *Get(TKey key) const {
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

    TValue &GetOrCreate(TKey key) {
        const auto l1Index = Level1Index(key);
        if (m_map[l1Index] == nullptr) {
            m_map[l1Index] = (Block *)m_allocator.AllocateRaw(sizeof(Block) * kL2Size);
            std::fill_n(m_map[l1Index], kL2Size, nullptr);
        }

        const auto l2Index = Level2Index(key);
        if (m_map[l1Index][l2Index] == nullptr) {
            m_map[l1Index][l2Index] = (TValue *)m_allocator.AllocateRaw(sizeof(TValue) * kL3Size);
            std::fill_n(m_map[l1Index][l2Index], kL3Size, TValue{});
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
    armajitto::memory::Allocator m_allocator;

    using Block = TValue *; // array of kL3Size TValues
    using Page = Block *;   // array of kL2Size Blocks
    Page *m_map = nullptr;  // array of kL1Size Pages

    static constexpr TKey Level1Index(TKey key) {
        return (key >> kL1Shift) & kL1Mask;
    }

    static constexpr TKey Level2Index(TKey key) {
        return (key >> kL2Shift) & kL2Mask;
    }

    static constexpr TKey Level3Index(TKey key) {
        return (key >> kL3Shift) & kL3Mask;
    }
};

} // namespace util
