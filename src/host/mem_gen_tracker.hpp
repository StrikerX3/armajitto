#pragma once

#include "core/allocator.hpp"
#include "util/pointer_cast.hpp"

#include <algorithm>
#include <cstdint>

namespace armajitto {

class MemoryGenerationTracker final {
public:
    static constexpr uint32_t kL1Bits = 12;
    static constexpr uint32_t kL2Bits = 12;
    static constexpr uint32_t kL3Bits = 6;
    static constexpr uint32_t kRemainingBits = 32 - (kL1Bits + kL2Bits + kL3Bits);

    static constexpr uint32_t kL1Size = 1u << kL1Bits;
    static constexpr uint32_t kL1Mask = kL1Size - 1u;
    static constexpr uint32_t kL1Shift = kL2Bits + kL3Bits + kRemainingBits;

    static constexpr uint32_t kL2Size = 1u << kL2Bits;
    static constexpr uint32_t kL2Mask = kL2Size - 1u;
    static constexpr uint32_t kL2Shift = kL3Bits + kRemainingBits;

    static constexpr uint32_t kL3Size = 1u << kL3Bits;
    static constexpr uint32_t kL3Mask = kL3Size - 1u;
    static constexpr uint32_t kL3Shift = kRemainingBits;

    static constexpr uint32_t kL1SplitThreshold = 16;
    static constexpr uint32_t kL2SplitThreshold = 32;

    static_assert(kL1SplitThreshold > 0 && kL1SplitThreshold <= 253,
                  "Level 1 split threshold must be between 1 and 253");
    static_assert(kL2SplitThreshold > 1 && kL2SplitThreshold <= 254,
                  "Level 2 split threshold must be between 2 and 254");
    static_assert(kL2SplitThreshold > kL1SplitThreshold,
                  "Level 2 split threshold must be greater than level 1 split threshold");

    struct Entry {
        uint32_t counter;
        uint32_t level;
    };

    Entry Get(uint32_t address) {
        const auto l1Index = Level1Index(address);
        auto &l1 = m_map[l1Index];
        if (l1.counter < kL1SplitThreshold) {
            return {l1.counter, 1};
        }
        if (l1.counter != 0xFF) {
            const auto counter = l1.counter;
            l1 = m_allocator.Allocate<L2Entry>();
            l1->fill({.counter = counter});
            return {counter, 2};
        }

        const auto l2Index = Level2Index(address);
        auto &l2 = (*l1)[l2Index];
        if (l2.counter < kL2SplitThreshold) {
            return {l2.counter, 2};
        }
        if (l2.counter != 0xFF) {
            const auto counter = l2.counter;
            l2 = m_allocator.Allocate<L3Entry>();
            l2->fill(counter);
            return {counter, 3};
        }

        const auto l3Index = Level3Index(address);
        return {(*l2)[l3Index], 3};
    }

    uint32_t GetLevel(uint32_t address) {
        const auto l1Index = Level1Index(address);
        auto &l1 = m_map[l1Index];
        if (l1.counter < kL1SplitThreshold) {
            return 1;
        }
        if (l1.counter != 0xFF) {
            const auto counter = l1.counter;
            l1 = m_allocator.Allocate<L2Entry>();
            l1->fill({.counter = counter});
        }

        const auto l2Index = Level2Index(address);
        auto &l2 = (*l1)[l2Index];
        if (l2.counter < kL2SplitThreshold) {
            return 2;
        }
        if (l2.counter != 0xFF) {
            const auto counter = l2.counter;
            l2 = m_allocator.Allocate<L3Entry>();
            l2->fill(counter);
        }

        return 3;
    }

    void Increment(uint32_t start, uint32_t end) {
        const auto l1StartIndex = Level1Index(start);
        const auto l1EndIndex = Level1Index(end);

        for (auto l1Index = l1StartIndex; l1Index <= l1EndIndex; l1Index++) {
            auto &l1 = m_map[l1Index];
            if (l1.counter == 0xFF) {
                const auto start2 = l1Index << kL1Shift;
                const auto end2 = start2 + (1 << kL1Shift);

                const auto l2StartIndex = Level2Index(start2);
                const auto l2EndIndex = Level2Index(end2);

                for (auto l2Index = l2StartIndex; l2Index <= l2EndIndex; l2Index++) {
                    auto &l2 = (*l1)[l1Index];
                    if (l2.counter == 0xFF) {
                        const auto start3 = l2Index << kL2Shift;
                        const auto end3 = start3 + (1 << kL2Shift);

                        const auto l3StartIndex = Level3Index(start3);
                        const auto l3EndIndex = Level3Index(end3);

                        for (auto l3Index = l3StartIndex; l3Index <= l3EndIndex; l3Index++) {
                            ++(*l2)[l3Index];
                        }
                    } else if (l2.counter != kL2SplitThreshold) {
                        if (++l2.counter == kL2SplitThreshold) {
                            l2 = m_allocator.Allocate<L3Entry>();
                            l2->fill(kL2SplitThreshold);
                        }
                    }
                }
            } else if (l1.counter < kL1SplitThreshold) {
                if (++l1.counter == kL1SplitThreshold) {
                    l1 = m_allocator.Allocate<L2Entry>();
                    l1->fill({.counter = kL1SplitThreshold});
                }
            }
        }
    }

    void Clear() {
        m_allocator.Release();
        m_map.fill({.counter = 0});
    }

    uintptr_t MapAddress() const {
        return CastUintPtr(&m_map);
    }

    template <typename T>
    union PackedCounterPointer {
        T *ptr;
        struct {
            uint8_t _padding[7];
            uint8_t counter;
        };

        PackedCounterPointer &operator=(T *ptr) {
            this->ptr = ptr;
            counter = 0xFF;
            return *this;
        }

        T &operator*() {
            return *reinterpret_cast<T *>(CastUintPtr(ptr) & ~0xFF000000'00000000ull);
        }

        const T &operator*() const {
            return *reinterpret_cast<const T *>(CastUintPtr(ptr) & ~0xFF000000'00000000ull);
        }

        T *operator->() {
            return reinterpret_cast<T *>(CastUintPtr(ptr) & ~0xFF000000'00000000ull);
        }

        const T *operator->() const {
            return reinterpret_cast<const T *>(CastUintPtr(ptr) & ~0xFF000000'00000000ull);
        }
    };

    using L3Entry = std::array<uint32_t, kL3Size>;
    using L2Entry = std::array<PackedCounterPointer<L3Entry>, kL2Size>;
    using L1Entry = std::array<PackedCounterPointer<L2Entry>, kL1Size>;
    L1Entry m_map;

    static constexpr uint32_t Level1Index(uint32_t address) {
        return (address >> kL1Shift) & kL1Mask;
    }

    static constexpr uint32_t Level2Index(uint32_t address) {
        return (address >> kL2Shift) & kL2Mask;
    }

    static constexpr uint32_t Level3Index(uint32_t address) {
        return (address >> kL3Shift) & kL3Mask;
    }

private:
    memory::Allocator m_allocator;
};

} // namespace armajitto
