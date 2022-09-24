#pragma once

#include "pointer_cast.hpp"

#include <array>
#include <concepts>
#include <cstdint>
#include <memory>
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
        Clear();
        delete[] m_map;
    }

    TValue *Get(TKey key) const {
        const auto l1Index = Level1Index(key);
        const auto l2Index = Level2Index(key);
        if (m_map[l1Index] == nullptr) {
            return nullptr;
        }
        return &m_map[l1Index][l2Index];
    }

    TValue &GetOrCreate(TKey key) {
        const auto l1Index = Level1Index(key);
        const auto l2Index = Level2Index(key);
        if (m_map[l1Index] == nullptr) {
            m_map[l1Index] = new TValue[kL2Size];
        }
        return m_map[l1Index][l2Index];
    }

    void Clear() {
        for (std::size_t i = 0; i < kL1Size; i++) {
            if (m_map[i] != nullptr) {
                delete[] m_map[i];
                m_map[i] = nullptr;
            }
        }
    }

    uintptr_t MapAddress() const {
        return CastUintPtr(m_map);
    }

private:
    using Page = TValue *; // array of kL2Size TValues
    Page *m_map = nullptr; // array of kL1Size Pages

    static constexpr TKey Level1Index(TKey key) {
        return (key >> kL1Shift) & kL1Mask;
    }

    static constexpr TKey Level2Index(TKey key) {
        return (key >> kL2Shift) & kL2Mask;
    }
};

} // namespace util
