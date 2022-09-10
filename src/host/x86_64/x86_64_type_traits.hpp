#pragma once

#include "util/type_traits.hpp"

#ifdef _WIN32
    #define NOMINMAX
#endif
#include <xbyak/xbyak.h>

#include <concepts>

namespace armajitto::detail {

// Define a concept encompassing all Xbyak register types
template <typename T>
concept XbyakReg = std::derived_from<T, Xbyak::Reg>;

// -----------------------------------------------------------------------------
// Compatibility overrides

// Make any Xbyak register compatible with any integral value
template <std::integral T, XbyakReg Reg>
struct is_compatible_base<T, Reg> {
    static constexpr bool value = true;
};

// Make any integral value compatible with any Xbyak register
template <XbyakReg Reg, std::integral T>
struct is_compatible_base<Reg, T> {
    static constexpr bool value = true;
};

} // namespace armajitto::detail
