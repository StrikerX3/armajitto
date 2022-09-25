#pragma once

#include "pointer_cast.hpp"

#include <array>
#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_set>
#include <utility>

namespace util {

template <std::integral TKey, typename TValue, TKey keyBits>
class TwoLevelArray final {
    static constexpr TKey kOne = static_cast<TKey>(1);

public:
    static constexpr auto kValueSize = sizeof(TValue);

    static constexpr TKey kKeyBits = keyBits;

    static constexpr TKey kL1Bits = kKeyBits >> kOne;
    static constexpr TKey kL1Size = kOne << kL1Bits;
    static constexpr TKey kL1Mask = kL1Size - 1;
    static constexpr TKey kL1Shift = kL1Bits;

    static constexpr TKey kL2Bits = kKeyBits - kL1Bits;
    static constexpr TKey kL2Size = kOne << kL2Bits;
    static constexpr TKey kL2Mask = kL2Size - 1;
    static constexpr TKey kL2Shift = 0;

    TwoLevelArray() {
        m_map = new Page[kL1Size];
        std::fill_n(m_map, kL1Size, nullptr);
    }

    ~TwoLevelArray() {
        FreeAll();
        delete[] m_map;
    }

    TValue *Get(TKey key) {
        const auto l1Index = Level1Index(key);
        const auto l2Index = Level2Index(key);
        if (m_map[l1Index] == nullptr) {
            return nullptr;
        }
        m_allocatedEntries.insert(key);
        return &m_map[l1Index][l2Index];
    }

    TValue &GetOrCreate(TKey key) {
        const auto l1Index = Level1Index(key);
        const auto l2Index = Level2Index(key);
        if (m_map[l1Index] == nullptr) {
            m_map[l1Index] = new TValue[kL2Size];
        }
        m_allocatedEntries.insert(key);
        return m_map[l1Index][l2Index];
    }

    void Clear() {
        for (auto entry : m_allocatedEntries) {
            const auto l1Index = Level1Index(entry);
            const auto l2Index = Level2Index(entry);
            m_map[l1Index][l2Index] = {};
        }
        m_allocatedEntries.clear();
    }

    void FreeAll() {
        for (auto entry : m_allocatedEntries) {
            const auto l1Index = Level1Index(entry);
            if (m_map[l1Index] != nullptr) {
                delete[] m_map[l1Index];
                m_map[l1Index] = nullptr;
            }
        }
        m_allocatedEntries.clear();
    }

    std::optional<TKey> LowerBound(TKey key) const {
        auto it = m_allocatedEntries.lower_bound(key);
        if (it != m_allocatedEntries.end()) {
            return *it;
        } else {
            return std::nullopt;
        }
    }

    uintptr_t MapAddress() const {
        return CastUintPtr(m_map);
    }

private:
    using Page = TValue *; // array of kL2Size TValues
    Page *m_map = nullptr; // array of kL1Size Pages

    std::unordered_set<TKey> m_allocatedEntries;

    static constexpr TKey Level1Index(TKey key) {
        return (key >> kL1Shift) & kL1Mask;
    }

    static constexpr TKey Level2Index(TKey key) {
        return (key >> kL2Shift) & kL2Mask;
    }
};

} // namespace util
