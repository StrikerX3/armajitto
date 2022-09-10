#pragma once

#include <type_traits>

namespace armajitto {

// Allows type-dependent validations in static_assert
template <typename T>
static constexpr bool alwaysFalse = false;

namespace detail {

    // Determines if a value of type U is can be passed as an argument of type T to a function
    template <typename T, typename U>
    struct is_compatible_base {
        static constexpr bool value = std::is_assignable_v<T, U> || std::is_same_v<T, U>;
    };

} // namespace detail

// Determines if a value of type U is can be passed as an argument of type T to a function
template <typename T, typename U>
struct is_compatible : public detail::is_compatible_base<std::remove_cvref_t<T>, std::remove_cvref_t<U>> {};

// nullptr is compatible with all pointer types.
template <typename T>
struct is_compatible<T *, std::nullptr_t> {
    static constexpr bool value = true;
};

// Convenience wrapper for is_compatible<T, U>::value
template <typename T, typename U>
constexpr bool is_compatible_v = is_compatible<T, U>::value;

namespace detail {
    template <typename... Ts>
    struct arg_list_base {};

    // Represents an empty parameter pack
    struct empty_arg_list : arg_list_base<> {
        static constexpr auto argCount = 0;
    };

    // Represents a parameter pack with multiple entries and allows extracting the first parameter type
    template <typename TFirst, typename... TRest>
    struct arg_list : arg_list_base<TFirst, TRest...> {
        using first = TFirst;
        using rest = arg_list<TRest...>;
        static constexpr auto argCount = sizeof...(TRest) + 1;
    };

    // Represents a parameter pack with one entry and allows extracting its type
    template <typename TFirst>
    struct arg_list<TFirst> : arg_list_base<TFirst> {
        using first = TFirst;
        using rest = empty_arg_list;
        static constexpr auto argCount = 1;
    };

    // Determines if the two argument lists are compatible
    template <typename FnArgs, typename Args>
    struct args_match {
        static constexpr bool value = FnArgs::argCount == Args::argCount &&
                                      is_compatible_v<typename FnArgs::first, typename Args::first> &&
                                      args_match<typename FnArgs::rest, typename Args::rest>::value;
    };

    // Determines if the two arguments are compatible
    template <typename FnArg, typename Arg>
    struct args_match<arg_list<FnArg>, arg_list<Arg>> {
        static constexpr bool value = is_compatible_v<FnArg, Arg>;
    };

    // Terminal match case where both argument lists are empty
    template <>
    struct args_match<empty_arg_list, empty_arg_list> {
        static constexpr bool value = true;
    };

    // Terminal mismatch case when there are more arguments provided than expected
    template <typename Arg>
    struct args_match<empty_arg_list, Arg> {
        static_assert(alwaysFalse<Arg>, "Too many arguments");
        static constexpr bool value = false;
    };

    // Terminal mismatch case where there are less arguments provided than expected
    template <typename Arg>
    struct args_match<Arg, empty_arg_list> {
        static_assert(alwaysFalse<Arg>, "Missing arguments");
        static constexpr bool value = false;
    };
} // namespace detail

// Wraps a parameter pack into a single object
template <typename... Ts>
struct arg_list {
    using type = detail::arg_list<Ts...>;
};

// Wraps an empty parameter pack into a single object
template <>
struct arg_list<> {
    using type = detail::empty_arg_list;
};

// Convenience wrapper for arg_list<Ts...>::type
template <typename... Ts>
using arg_list_t = typename arg_list<Ts...>::type;

// Determines if the two argument lists are compatible
template <typename FnArgs, typename Args>
struct args_match : public detail::args_match<FnArgs, Args> {};

template <typename FnArgs, typename Args>
static constexpr bool args_match_v = args_match<FnArgs, Args>::value;

} // namespace armajitto
