#pragma once

#include <bit>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace bit {

// Returns true if the value has the specified bit set
template <size_t bit, typename T>
constexpr bool test(T value) {
    static_assert(bit < sizeof(T) * 8, "Bit out of range");
    return value & (1 << bit);
}

// Returns true if the value has the specified bit set
template <typename T>
constexpr bool test(size_t bit, T value) {
    assert(bit < sizeof(T) * 8 && "Bit out of range");
    return value & (1 << bit);
}

// Extracts a range of bits from the value
template <size_t offset, size_t length = 1, typename T>
constexpr T extract(T value) {
    static_assert(offset < sizeof(T) * 8, "Offset out of range");
    static_assert(length > 0, "Length cannot be zero");
    static_assert(offset + length <= sizeof(T) * 8, "Length exceeds capacity");

    using UT = std::make_unsigned_t<T>;

    constexpr UT mask = static_cast<UT>(~(~0 << length));
    return (value >> offset) & mask;
}

// Sign-extend from a constant bit width
template <unsigned B, std::integral T>
constexpr auto sign_extend(const T x) {
    using ST = std::make_signed_t<T>;
    struct {
        ST x : B;
    } s{static_cast<ST>(x)};
    return s.x;
}

// Finds the smallest integral power of two not less than the given value
template <typename T>
constexpr T bitceil(T value) {
    if (value == 0) {
        return 1;
    }
    return static_cast<T>(T{1} << (std::numeric_limits<T>::digits - std::countl_zero(static_cast<T>(value - 1))));
}

} // namespace bit
