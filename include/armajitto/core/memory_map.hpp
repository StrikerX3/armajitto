#pragma once

#include "armajitto/util/bit_ops.hpp"
#include "armajitto/util/pointer_cast.hpp"

#include <bit>
#include <cstdint>

namespace armajitto {

class MemoryMap {
public:
    MemoryMap(uint32_t pageSize);
    ~MemoryMap();

    void Map(uint32_t baseAddress, uint32_t size, uint8_t *ptr);
    void Unmap(uint32_t baseAddress, uint32_t size);

    void Clear();
    void FreeEmptyPages();

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
};

} // namespace armajitto
