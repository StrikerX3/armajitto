#include "armajitto/core/memory_map.hpp"

#include <cassert>

namespace armajitto {

MemoryMap::MemoryMap(uint32_t pageSize)
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

MemoryMap::~MemoryMap() {
    Clear();
    delete[] m_map;
}

void MemoryMap::Map(uint32_t baseAddress, uint32_t size, uint8_t *ptr) {
    assert((baseAddress & m_pageMask) == 0); // baseAddress must be page-aligned
    assert((size & m_pageMask) == 0);        // size must be page-aligned

    const uint32_t numPages = size >> m_pageShift;
    const uint32_t startPage = baseAddress >> m_pageShift;
    const uint32_t endPage = startPage + numPages;
    for (uint32_t page = startPage; page < endPage; page++) {
        const uint32_t pageIndex = (page >> m_l2Bits) & m_l1Mask;
        const uint32_t entryIndex = page & m_l2Mask;
        const uint32_t offset = page << m_pageShift;
        if (m_map[pageIndex] == nullptr) {
            m_map[pageIndex] = new Entry[m_l2Size];
        }
        m_map[pageIndex][entryIndex] = ptr + offset;
    }
}

void MemoryMap::Unmap(uint32_t baseAddress, uint32_t size) {
    assert((baseAddress & m_pageMask) == 0); // baseAddress must be page-aligned
    assert((size & m_pageMask) == 0);        // size must be page-aligned

    const uint32_t numPages = size >> m_pageShift;
    const uint32_t startPage = baseAddress >> m_pageShift;
    const uint32_t endPage = startPage + numPages;
    for (uint32_t page = startPage; page < endPage; page++) {
        const uint32_t pageIndex = (page >> m_l2Bits) & m_l1Mask;
        const uint32_t entryIndex = page & m_l2Mask;
        if (m_map[pageIndex] != nullptr) {
            m_map[pageIndex][entryIndex] = nullptr;
        }
    }
}

void MemoryMap::Clear() {
    for (size_t i = 0; i < m_l1Size; i++) {
        if (m_map[i] != nullptr) {
            delete[] m_map[i];
        }
        m_map[i] = nullptr;
    }
}

void MemoryMap::FreeEmptyPages() {
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

} // namespace armajitto
