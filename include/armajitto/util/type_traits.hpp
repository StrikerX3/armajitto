#pragma once

#include <type_traits>

namespace armajitto {

// Allows type-dependent validations in static_assert
template <typename T>
static constexpr bool alwaysFalse = false;

// Combines std::remove_cv_t and std::remove_reference_t
template <typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

// Determines if a value of type U is can be passed as an argument of type T to a function
template <typename T, typename U>
constexpr bool is_compatible_v = std::is_assignable_v<T, U> || std::is_same_v<remove_cvref_t<T>, remove_cvref_t<U>>;

template <typename... Ts>
struct arg_list_base {};

// Extracts the first parameter from a template parameter pack
template <typename TFirst, typename... TRest>
struct arg_list : arg_list_base<TFirst, TRest...> {
    using First = TFirst;
    using Rest = arg_list<TRest...>;
    static constexpr auto argCount = sizeof...(TRest) + 1;
};

// Extracts the only parameter from the template parameter
template <typename TFirst>
struct arg_list<TFirst> : arg_list_base<TFirst> {
    using First = TFirst;
    static constexpr auto argCount = 1;
};

// Determines if the two argument lists are compatible
template <typename FnArgs, typename Args>
struct args_match {
    static constexpr bool value = is_compatible_v<typename FnArgs::First, typename Args::First> &&
                                  FnArgs::argCount == Args::argCount &&
                                  args_match<typename FnArgs::Rest, typename Args::Rest>::value;
};

// Determines if the two arguments are compatible
template <typename FnArg, typename Arg>
struct args_match<arg_list<FnArg>, arg_list<Arg>> {
    static constexpr bool value = is_compatible_v<FnArg, Arg>;
};

template <typename FnArgs, typename Args>
static constexpr bool args_match_v = args_match<FnArgs, Args>::value;

} // namespace armajitto
