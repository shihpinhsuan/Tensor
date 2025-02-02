//
//  C++17.hpp
//  Tensor
//
//  Created by 陳均豪 on 2022/1/29.
//

#ifndef C__17_hpp
#define C__17_hpp

#include <type_traits>
#include <utility>
#include <memory>
#include <sstream>
#include <string>
#include <cstdlib>
#include <functional>

namespace otter {


#ifdef __cpp_lib_apply

template <class F, class Tuple>
inline constexpr decltype(auto) apply(F&& f, Tuple&& t) {
    return std::apply(std::forward<F>(f), std::forward<Tuple>(t));
}

#else

namespace detail {
template <class F, class Tuple, std::size_t... INDEX>
#if defined(_MSC_VER)
constexpr auto apply_impl(F&& f, Tuple&& t, std::index_sequence<INDEX...>)
#else
constexpr decltype(auto) apply_impl(F&& f, Tuple&& t, std::index_sequence<INDEX...>)
#endif
{
    return std::forward<F>(f)(std::get<INDEX>(std::forward<Tuple>(t))...);
}
}  // namespace detail

template <class F, class Tuple>
constexpr decltype(auto) apply(F&& f, Tuple&& t) {
    return detail::apply_impl(
        std::forward<F>(f), std::forward<Tuple>(t),
        std::make_index_sequence<std::tuple_size<std::remove_reference_t<Tuple>>::value>{});
}

#endif

#ifdef __cpp_lib_void_t

template <class T>
using void_t = std::void_t<T>;

#else

// Implementation taken from http://en.cppreference.com/w/cpp/types/void_t
// (it takes CWG1558 into account and also works for older compilers)
template <typename... Ts>
struct make_void {
    typedef void type;
};
template <typename... Ts>
using void_t = typename make_void<Ts...>::type;

#endif

namespace detail {
struct _identity final {
    template <class T>
    using type_identity = T;

    template <class T>
    decltype(auto) operator()(T&& arg) {
        return std::forward<T>(arg);
    }
};

template <class Func, class Enable = void>
struct function_takes_identity_argument : std::false_type {};
#if defined(_MSC_VER)
// For some weird reason, MSVC shows a compiler error when using guts::void_t
// instead of std::void_t. But we're only building on MSVC versions that have
// std::void_t, so let's just use that one.
template <class Func>
struct function_takes_identity_argument<
    Func,
    std::void_t<decltype(std::declval<Func>()(_identity()))>> : std::true_type {
};
#else
template <class Func>
struct function_takes_identity_argument<
    Func,
    void_t<decltype(std::declval<Func>()(_identity()))>> : std::true_type {};
#endif

template <bool Condition>
struct _if_constexpr;

template <>
struct _if_constexpr<true> final {
  template <
      class ThenCallback,
      class ElseCallback,
      std::enable_if_t<
          function_takes_identity_argument<ThenCallback>::value,
          void*> = nullptr>
  static decltype(auto) call(
      ThenCallback&& thenCallback,
      ElseCallback&& /* elseCallback */) {
    // The _identity instance passed in can be used to delay evaluation of an
    // expression, because the compiler can't know that it's just the identity
    // we're passing in.
    return thenCallback(_identity());
  }

  template <
      class ThenCallback,
      class ElseCallback,
      std::enable_if_t<
          !function_takes_identity_argument<ThenCallback>::value,
          void*> = nullptr>
  static decltype(auto) call(
      ThenCallback&& thenCallback,
      ElseCallback&& /* elseCallback */) {
    return thenCallback();
  }
};

template <>
struct _if_constexpr<false> final {
  template <
      class ThenCallback,
      class ElseCallback,
      std::enable_if_t<
          function_takes_identity_argument<ElseCallback>::value,
          void*> = nullptr>
  static decltype(auto) call(
      ThenCallback&& /* thenCallback */,
      ElseCallback&& elseCallback) {
    // The _identity instance passed in can be used to delay evaluation of an
    // expression, because the compiler can't know that it's just the identity
    // we're passing in.
    return elseCallback(_identity());
  }

  template <
      class ThenCallback,
      class ElseCallback,
      std::enable_if_t<
          !function_takes_identity_argument<ElseCallback>::value,
          void*> = nullptr>
  static decltype(auto) call(
      ThenCallback&& /* thenCallback */,
      ElseCallback&& elseCallback) {
    return elseCallback();
  }
};
} // namespace detail

/*
 * Get something like C++17 if constexpr in C++14.
 *
 * Example 1: simple constexpr if/then/else
 *   template<int arg> int increment_absolute_value() {
 *     int result = arg;
 *     if_constexpr<(arg > 0)>(
 *       [&] { ++result; }  // then-case
 *       [&] { --result; }  // else-case
 *     );
 *     return result;
 *   }
 *
 * Example 2: without else case (i.e. conditionally prune code from assembly)
 *   template<int arg> int decrement_if_positive() {
 *     int result = arg;
 *     if_constexpr<(arg > 0)>(
 *       // This decrement operation is only present in the assembly for
 *       // template instances with arg > 0.
 *       [&] { --result; }
 *     );
 *     return result;
 *   }
 *
 * Example 3: branch based on type (i.e. replacement for SFINAE)
 *   struct MyClass1 {int value;};
 *   struct MyClass2 {int val};
 *   template <class T>
 *   int func(T t) {
 *     return if_constexpr<std::is_same<T, MyClass1>::value>(
 *       [&](auto _) { return _(t).value; }, // this code is invalid for T ==
 * MyClass2, so a regular non-constexpr if statement wouldn't compile
 *       [&](auto _) { return _(t).val; }    // this code is invalid for T ==
 * MyClass1
 *     );
 *   }
 *
 * Note: The _ argument passed in Example 3 is the identity function, i.e. it
 * does nothing. It is used to force the compiler to delay type checking,
 * because the compiler doesn't know what kind of _ is passed in. Without it,
 * the compiler would fail when you try to access t.value but the member doesn't
 * exist.
 *
 * Note: In Example 3, both branches return int, so func() returns int. This is
 * not necessary. If func() had a return type of "auto", then both branches
 * could return different types, say func<MyClass1>() could return int and
 * func<MyClass2>() could return string.
 *
 * Note: if_constexpr<cond, t, f> is *eager* w.r.t. template expansion - meaning
 * this polyfill does not behave like a true "if statement at compilation time".
 *       The `_` trick above only defers typechecking, which happens after
 * templates have been expanded. (Of course this is all that's necessary for
 * many use cases).
 */
template <bool Condition, class ThenCallback, class ElseCallback>
decltype(auto) if_constexpr(
    ThenCallback&& thenCallback,
    ElseCallback&& elseCallback) {
#if defined(__cpp_if_constexpr)
  // If we have C++17, just use it's "if constexpr" feature instead of wrapping
  // it. This will give us better error messages.
  if constexpr (Condition) {
    if constexpr (detail::function_takes_identity_argument<
                      ThenCallback>::value) {
      return ::std::forward<ThenCallback>(thenCallback)(detail::_identity());
    } else {
      return ::std::forward<ThenCallback>(thenCallback)();
    }
  } else {
    if constexpr (detail::function_takes_identity_argument<
                      ElseCallback>::value) {
      return ::std::forward<ElseCallback>(elseCallback)(detail::_identity());
    } else {
      return ::std::forward<ElseCallback>(elseCallback)();
    }
  }
#else
  // C++14 implementation of if constexpr
  return detail::_if_constexpr<Condition>::call(
      ::std::forward<ThenCallback>(thenCallback),
      ::std::forward<ElseCallback>(elseCallback));
#endif
}

template <bool Condition, class ThenCallback>
decltype(auto) if_constexpr(ThenCallback&& thenCallback) {
#if defined(__cpp_if_constexpr)
  // If we have C++17, just use it's "if constexpr" feature instead of wrapping
  // it. This will give us better error messages.
  if constexpr (Condition) {
    if constexpr (detail::function_takes_identity_argument<
                      ThenCallback>::value) {
      return ::std::forward<ThenCallback>(thenCallback)(detail::_identity());
    } else {
      return ::std::forward<ThenCallback>(thenCallback)();
    }
  }
#else
  // C++14 implementation of if constexpr
  return if_constexpr<Condition>(
      ::std::forward<ThenCallback>(thenCallback), [](auto) {});
#endif
}



}   // end namespace otter

#endif /* C__17_hpp */
