/*
Type-safe enum class bitmasks.
Based on work by Andre Haupt from his blog at
http://blog.bitwigglers.org/using-enum-classes-as-type-safe-bitmasks/
To enable enum classes to be used as bitmasks, use the ENABLE_BITMASK_OPERATORS
macro:
  enum class MyBitmask {
     None = 0b0000,
     One = 0b0001,
     Two = 0b0010,
     Three = 0b0100,
  };
  ENABLE_BITMASK_OPERATORS(MyBitmask)
From now on, MyBitmask's values can be used with bitwise operators.
You may find it cumbersome to check for the presence or absence of specific
values in enum class bitmasks. For example:
  MyBitmask bm = ...;
  MyBitmask oneAndThree = (MyBitmask::One | MyBitmask::Three);
  // Check if either bit one or three is set
  if (bm & oneAndThree != MyBitmask::None) {
      ...
  }
  // Check if both bits one and three are set
  if (bm & oneAndThree == oneAndThree) {
      ...
  }
To help with that, you can wrap the bitmask into the BitmaskEnum type, which
provides a set of bitmask checks and useful conversions:
  MyBitmask bm = ...;
  MyBitmask oneAndThree = (MyBitmask::One | MyBitmask::Three);
  auto wbm = BitmaskEnum(bm);
  // Check if either bit one or three is set
  if (bm.AnyOf(oneAndThree)) {
      ...
  }
  // Check if both bits one and three are set
  if (bm.AllOf(oneAndThree)) {
      ...
  }
  // Check if any bit is set
  if (bm) {
      ...
  }
  // Convert back to the enum class
  MyBitmask backToEnum = wbm;
*/
#pragma once

#include <type_traits>

#define ENABLE_BITMASK_OPERATORS(x)      \
    template <>                          \
    struct is_bitmask_enum<x> {          \
        static const bool enable = true; \
    };

template <typename Enum>
struct is_bitmask_enum {
    static const bool enable = false;
};

template <class Enum>
constexpr bool is_bitmask_enum_v = is_bitmask_enum<Enum>::enable;

// ----- Bitwise operators ----------------------------------------------------

template <typename Enum>
constexpr typename std::enable_if_t<is_bitmask_enum_v<Enum>, Enum> operator|(Enum lhs, Enum rhs) noexcept {
    using underlying = typename std::underlying_type_t<Enum>;
    return static_cast<Enum>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

template <typename Enum>
constexpr typename std::enable_if_t<is_bitmask_enum_v<Enum>, Enum> operator&(Enum lhs, Enum rhs) noexcept {
    using underlying = typename std::underlying_type_t<Enum>;
    return static_cast<Enum>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

template <typename Enum>
constexpr typename std::enable_if_t<is_bitmask_enum_v<Enum>, Enum> operator^(Enum lhs, Enum rhs) noexcept {
    using underlying = typename std::underlying_type_t<Enum>;
    return static_cast<Enum>(static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs));
}

template <typename Enum>
constexpr typename std::enable_if_t<is_bitmask_enum_v<Enum>, Enum> operator~(Enum rhs) noexcept {
    using underlying = typename std::underlying_type_t<Enum>;
    return static_cast<Enum>(~static_cast<underlying>(rhs));
}

// ----- Bitwise assignment operators -----------------------------------------

template <typename Enum>
constexpr typename std::enable_if_t<is_bitmask_enum_v<Enum>, Enum> operator|=(Enum &lhs, Enum rhs) noexcept {
    using underlying = typename std::underlying_type_t<Enum>;
    lhs = static_cast<Enum>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
    return lhs;
}

template <typename Enum>
constexpr typename std::enable_if_t<is_bitmask_enum_v<Enum>, Enum> operator&=(Enum &lhs, Enum rhs) noexcept {
    using underlying = typename std::underlying_type_t<Enum>;
    lhs = static_cast<Enum>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
    return lhs;
}

template <typename Enum>
constexpr typename std::enable_if_t<is_bitmask_enum_v<Enum>, Enum> operator^=(Enum &lhs, Enum rhs) noexcept {
    using underlying = typename std::underlying_type_t<Enum>;
    lhs = static_cast<Enum>(static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs));
    return lhs;
}

// ----- Bitwise mask checks --------------------------------------------------

template <typename Enum>
struct BitmaskEnum {
    const Enum value;
    static const Enum none = static_cast<Enum>(0);

    using underlying = typename std::underlying_type_t<Enum>;

    constexpr BitmaskEnum(Enum value) noexcept
        : value(value) {
        static_assert(is_bitmask_enum_v<Enum>);
    }

    // Convert back to enum if required
    constexpr operator Enum() const noexcept { return value; }

    // Convert to true if there is any bit set in the bitmask
    constexpr operator bool() const noexcept { return Any(); }

    // Returns true if any bit is set
    [[nodiscard]] constexpr bool Any() const noexcept { return value != none; }

    // Returns true if all bits are clear
    [[nodiscard]] constexpr bool None() const noexcept { return value == none; }

    // Returns true if any bit in the given mask is set
    [[nodiscard]] constexpr bool AnyOf(const Enum &mask) const noexcept { return (value & mask) != none; }

    // Returns true if all bits in the given mask are set
    [[nodiscard]] constexpr bool AllOf(const Enum &mask) const noexcept { return (value & mask) == mask; }

    // Returns true if none of the bits in the given mask are set
    [[nodiscard]] constexpr bool NoneOf(const Enum &mask) const noexcept { return (value & mask) == none; }

    // Returns true if any bits excluding the mask are set
    [[nodiscard]] constexpr bool AnyExcept(const Enum &mask) const noexcept { return (value & ~mask) != none; }

    // Returns true if no bits excluding the mask are set
    [[nodiscard]] constexpr bool NoneExcept(const Enum &mask) const noexcept { return (value & ~mask) == none; }
};
