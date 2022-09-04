#pragma once

#include <functional>
#include <type_traits>
#include <utility>

namespace util {

namespace detail {

    template <typename T>
    constexpr bool alwaysFalse = false;

    template <typename TReturn, typename... TArgs>
    struct FuncClass {
        using ReturnType = TReturn;
        using FnType = ReturnType (*)(void *context, TArgs... args);

        FuncClass()
            : m_context(nullptr)
            , m_fn(nullptr) {}

        FuncClass(void *context, FnType fn)
            : m_context(context)
            , m_fn(fn) {}

        FuncClass(const FuncClass &) = default;
        FuncClass(FuncClass &&) = default;

        FuncClass &operator=(const FuncClass &) = default;
        FuncClass &operator=(FuncClass &&) = default;

        void Rebind(void *context, FnType fn) {
            m_context = context;
            m_fn = fn;
        }

        ReturnType operator()(TArgs... args) {
            if (m_fn != nullptr) {
                return m_fn(m_context, std::forward<TArgs>(args)...);
            } else if constexpr (!std::is_void_v<ReturnType>) {
                return {};
            }
        }

    private:
        void *m_context;
        FnType m_fn;
    };

    template <typename TFunc>
    struct FuncImpl {
        static_assert(alwaysFalse<TFunc>, "Callback requires a function argument");
    };

    template <typename TReturn, typename... TArgs>
    struct FuncImpl<TReturn(TArgs...)> {
        using type = FuncClass<TReturn, TArgs...>;
    };

} // namespace detail

template <typename TFunc>
class Callback : public detail::FuncImpl<TFunc>::type {
    using FnType = typename detail::FuncImpl<TFunc>::type::FnType;

public:
    Callback() = default;

    Callback(void *context, FnType fn)
        : detail::FuncImpl<TFunc>::type(context, fn) {}

    Callback(const Callback &) = default;
    Callback(Callback &&) = default;

    Callback &operator=(const Callback &) = default;
    Callback &operator=(Callback &&) = default;
};

// -------------------------------------------------------------------------------------------------

namespace detail {

    template <typename>
    struct MFPCallbackMaker;

    template <typename Return, typename Object, typename... Args>
    struct MFPCallbackMaker<Return (Object::*)(Args...)> {
        using class_type = Object;

        template <Return (Object::*mfp)(Args...)>
        static auto GetCallback(Object *context) {
            return Callback<Return(Args...)>{context, [](void *context, Args... args) {
                                                 auto &obj = *static_cast<Object *>(context);
                                                 return (obj.*mfp)(std::forward<Args>(args)...);
                                             }};
        }
    };

    template <typename Return, typename Object, typename... Args>
    struct MFPCallbackMaker<Return (Object::*)(Args...) const> {
        using class_type = Object;

        template <Return (Object::*mfp)(Args...)>
        static auto GetCallback(Object *context) {
            return Callback<Return(Args...)>{context, [](void *context, Args... args) {
                                                 auto &obj = *static_cast<Object *>(context);
                                                 return (obj.*mfp)(std::forward<Args>(args)...);
                                             }};
        }
    };

} // namespace detail

template <auto mfp, typename = std::enable_if_t<std::is_member_function_pointer_v<decltype(mfp)>>>
auto MakeClassMemberCallback(typename detail::MFPCallbackMaker<decltype(mfp)>::class_type *context) {
    return detail::MFPCallbackMaker<decltype(mfp)>::template GetCallback<mfp>(context);
}

} // namespace util
