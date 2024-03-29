#pragma once

#include "bit_ops.hpp"
#include "noitree.hpp"
#include "pointer_cast.hpp"

#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <map>

namespace util {

// A memory map consisting of multiple stacked layers of memory maps.
//
// Layers with larger indices are overlaid on top of those with lower indices.
// The class automatically manages and builds an effective page map based on these overlaid layers.
//
// This allows simple and efficient memory pointer queries, and easy management of multiple layers of memory maps such
// as those used in complex systems with caches overlaid on top of the base system memory view.
template <size_t numLayers, typename TAttrs>
class LayeredMemoryMap {
public:
    LayeredMemoryMap(uint32_t pageSize)
        : m_pageSize(bit::bitceil(pageSize))
        , m_pageMask(m_pageSize - 1)
        , m_pageShift(std::countr_zero(m_pageSize))
        , m_lutBits(32 - m_pageShift)
        , m_l1Bits(m_lutBits / 2)
        , m_l1Size(1u << m_l1Bits)
        , m_l1Mask(m_l1Size - 1)
        , m_l1Shift(32 - m_l1Bits)
        , m_l2Bits(m_lutBits - m_l1Bits)
        , m_l2Size(1u << m_l2Bits)
        , m_l2Mask(m_l2Size - 1)
        , m_l2Shift(32 - m_lutBits) {

        m_map = new Page[m_l1Size];
        std::fill_n(m_map, m_l1Size, nullptr);
    }

    ~LayeredMemoryMap() {
        Clear();
        delete[] m_map;
    }

    void Map(uint8_t layer, uint32_t baseAddress, uint64_t size, TAttrs attrs, uint8_t *ptr,
             uint64_t mirrorSize = 0x1'0000'0000) {
        if (size == 0) {
            return;
        }
        if (mirrorSize == 0) {
            mirrorSize = size;
        }
        assert(layer < numLayers);               // layer index must be in-bounds
        assert((baseAddress & m_pageMask) == 0); // baseAddress must be page-aligned
        assert((size & m_pageMask) == 0);        // size must be page-aligned
        assert(std::has_single_bit(mirrorSize)); // mirrorSize must be a power of two
        // ptr may be null, which can be used to assign attributes to MMIO ranges

        const uint64_t finalAddress = (uint64_t)baseAddress + size;
        for (uint64_t address = baseAddress; address < finalAddress; address += mirrorSize) {
            const uint32_t blockSize = std::min((uint64_t)mirrorSize, finalAddress - address);
            DoMap(layer, address, blockSize, mirrorSize - 1, attrs, ptr);
        }
    }

    void Unmap(uint8_t layer, uint32_t baseAddress, uint64_t size) {
        if (size == 0) {
            return;
        }
        assert(layer < numLayers);               // layer index must be in-bounds
        assert((baseAddress & m_pageMask) == 0); // baseAddress must be page-aligned
        assert((size & m_pageMask) == 0);        // size must be page-aligned

        const uint64_t finalAddress = (uint64_t)baseAddress + size - 1;

        uint32_t address = baseAddress;
        while (address <= finalAddress) {
            if (auto range = m_layers[layer].LowerBound(baseAddress)) {
                auto [lb, ub] = *range;
                const uint32_t startAddress = std::max(address, lb);
                const uint32_t endAddress = std::min((uint64_t)ub, finalAddress);
                UnmapSubrange(layer, startAddress, endAddress - startAddress + 1);
                address = ub;
            } else {
                break;
            }
        }
    }

    void Clear() {
        for (size_t i = 0; i < m_l1Size; i++) {
            if (m_map[i] != nullptr) {
                delete[] m_map[i];
            }
            m_map[i] = nullptr;
        }
        for (auto &layer : m_layers) {
            layer.Clear();
        }
    }

    void FreeEmptyPages() {
        for (size_t i = 0; i < m_l1Size; i++) {
            if (m_map[i] != nullptr) {
                bool allEmpty = true;
                for (size_t j = 0; j < m_l2Size; j++) {
                    if (m_map[i][j] != nullptr) {
                        allEmpty = false;
                        break;
                    }
                }
                if (allEmpty) {
                    delete[] m_map[i];
                    m_map[i] = nullptr;
                }
            }
            m_map[i] = nullptr;
        }
    }

    uintptr_t GetL1MapAddress() const {
        return CastUintPtr(m_map);
    }

    uint32_t GetL1Shift() const {
        return m_l1Shift;
    }

    uint32_t GetL2Shift() const {
        return m_l2Shift;
    }

    uint32_t GetL2Mask() const {
        return m_l2Mask;
    }

    uint32_t GetPageMask() const {
        return m_pageMask;
    }

    template <typename T>
    T *GetPointer(uint32_t address) {
        // Get level 1 pointer
        const uint32_t l1Index = address >> m_l1Shift;
        auto *l1Ptr = m_map[l1Index];
        if (l1Ptr == nullptr) {
            return nullptr;
        }

        // Get level 2 pointer
        const uint32_t l2Index = (address >> m_l2Shift) & m_l2Mask;
        auto *l2Ptr = l1Ptr[l2Index];
        if (l2Ptr == nullptr) {
            return nullptr;
        }

        // Read from selected page
        const uint32_t offset = address & m_pageMask;
        return static_cast<T *>(static_cast<void *>(&static_cast<uint8_t *>(l2Ptr)[offset]));
    }

    bool IsMapped(uint32_t address) {
        // Get level 1 pointer
        const uint32_t l1Index = address >> m_l1Shift;
        auto *l1Ptr = m_map[l1Index];
        if (l1Ptr == nullptr) {
            return false;
        }

        // Get level 2 pointer
        const uint32_t l2Index = (address >> m_l2Shift) & m_l2Mask;
        auto *l2Ptr = l1Ptr[l2Index];
        if (l2Ptr == nullptr) {
            return false;
        }

        // Page is mapped
        return true;
    }

    TAttrs GetAttributes(uint32_t address) {
        for (size_t layer = numLayers - 1; layer < numLayers; --layer) {
            if (m_layers[layer].Contains(address)) {
                return m_layers[layer].At(address).attrs;
            }
        }
        return {};
    }

private:
    const uint32_t m_pageSize;
    const uint32_t m_pageMask;
    const uint32_t m_pageShift;

    const uint32_t m_lutBits;

    const uint32_t m_l1Bits;
    const uint32_t m_l1Size;
    const uint32_t m_l1Mask;
    const uint32_t m_l1Shift;

    const uint32_t m_l2Bits;
    const uint32_t m_l2Size;
    const uint32_t m_l2Mask;
    const uint32_t m_l2Shift;

    using Entry = void *;
    using Page = Entry *;  // array of Entry
    Page *m_map = nullptr; // array of Page

    void DoMap(uint8_t layer, uint32_t baseAddress, uint64_t size, uint32_t mask, TAttrs attrs, uint8_t *ptr) {
        const uint32_t finalAddress = baseAddress + size - 1;
        m_layers[layer].Insert(baseAddress, finalAddress, {ptr, mask, attrs});

        uint32_t address = baseAddress;
        while (address <= finalAddress) {
            // Skip ranges in layers above the current layer
            const uint32_t fillStart = address;
            bool first = true;
            uint64_t fillEnd = finalAddress + 1;
            uint64_t nextAddress = finalAddress + 1;
            for (uint32_t nextLayer = layer + 1; nextLayer < numLayers; nextLayer++) {
                if (auto range = m_layers[nextLayer].LowerBound(fillStart)) {
                    auto [lb, ub] = *range;
                    fillEnd = std::min(fillEnd, (uint64_t)lb);
                    if (first) {
                        nextAddress = ub;
                    } else {
                        nextAddress = std::max(nextAddress, (uint64_t)ub);
                    }
                }
            }
            if (fillStart < fillEnd) {
                if (ptr != nullptr) {
                    const uint32_t offset = fillStart - baseAddress;
                    SetRange(fillStart, fillEnd - fillStart, mask, ptr + offset);
                } else {
                    SetRange(fillStart, fillEnd - fillStart, mask, nullptr);
                }
            }
            address = nextAddress + 1;
        }
    }

    void SetRange(uint32_t baseAddress, uint64_t size, uint32_t mask, uint8_t *ptr, uint32_t initialOffset = 0) {
        const uint32_t numPages = size >> m_pageShift;
        const uint32_t startPage = baseAddress >> m_pageShift;
        const uint32_t endPage = startPage + numPages;
        for (uint32_t page = startPage; page < endPage; page++) {
            const uint32_t pageIndex = (page >> m_l2Bits) & m_l1Mask;
            const uint32_t entryIndex = page & m_l2Mask;
            if (m_map[pageIndex] == nullptr) {
                m_map[pageIndex] = new Entry[m_l2Size];
                std::fill_n(m_map[pageIndex], m_l2Size, nullptr);
            }
            if (ptr != nullptr) {
                const uint32_t offset = (page - startPage) << m_pageShift;
                m_map[pageIndex][entryIndex] = ptr + ((initialOffset + offset) & mask);
            } else {
                m_map[pageIndex][entryIndex] = nullptr;
            }
        }
    }

    void UnmapSubrange(uint8_t layer, uint32_t baseAddress, uint64_t size) {
        const uint32_t finalAddress = baseAddress + size - 1;
        m_layers[layer].Remove(baseAddress, finalAddress);

        uint32_t address = baseAddress;
        while (address <= finalAddress) {
            // Skip ranges in layers above the current layer
            const uint32_t fillStart = address;
            bool first = true;
            uint64_t fillEnd = finalAddress + 1;
            uint64_t nextAddress = finalAddress + 1;
            for (uint32_t nextLayer = layer + 1; nextLayer < numLayers; nextLayer++) {
                if (auto range = m_layers[nextLayer].LowerBound(fillStart)) {
                    auto [lb, ub] = *range;
                    fillEnd = std::min(fillEnd, (uint64_t)lb);
                    if (first) {
                        nextAddress = ub;
                    } else {
                        nextAddress = std::max(nextAddress, (uint64_t)ub);
                    }
                }
            }

            if (fillStart < fillEnd) {
                // Map ranges from layers below the current layer, or nullptr if empty
                uint8_t *ptr = nullptr;
                uint32_t mask = 0;
                uint32_t offset = 0;
                for (int32_t prevLayer = layer - 1; prevLayer >= 0; prevLayer--) {
                    if (auto range = m_layers[prevLayer].LowerBound(fillStart)) {
                        auto [lb, ub] = *range;
                        if (lb > fillStart) {
                            fillEnd = std::min(fillEnd, (uint64_t)lb);
                            nextAddress = fillEnd - 1;
                        } else if (fillStart <= ub) {
                            fillEnd = std::min(fillEnd, (uint64_t)ub + 1);
                            nextAddress = fillEnd - 1;
                            auto layerEntry = m_layers[prevLayer].At(lb);
                            ptr = layerEntry.ptr;
                            mask = layerEntry.mask;
                            offset = fillStart - lb;
                            if (fillStart >= lb) {
                                break;
                            }
                        }
                    }
                }
                SetRange(fillStart, fillEnd - fillStart, mask, ptr, offset);
            }
            address = nextAddress + 1;
        }
    }

    // -----------------------------------------------------------------------------------------------------------------

    struct LayerEntry {
        uint8_t *ptr;
        uint32_t mask;
        TAttrs attrs;

        bool operator==(const LayerEntry &) const = default;
    };

    using Layer = util::NonOverlappingIntervalTree<uint32_t, LayerEntry>;
    std::array<Layer, numLayers> m_layers;

    static uint64_t MakeKey(uint8_t layer, uint32_t baseAddress) {
        return ((uint64_t)baseAddress << 8ull) | ~layer;
    }

    static uint8_t LayerFromKey(uint64_t key) {
        return ~key;
    }

    static uint32_t AddressFromKey(uint64_t key) {
        return key >> 8ull;
    }
};

} // namespace util
