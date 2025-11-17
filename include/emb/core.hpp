#pragma once

#ifdef __TMS320C28XX__
#define __c28x__
#endif

#if !defined(__c28x__) && !defined(__arm__) && !defined(__x86_64__)
#error "emblib: arch not recognized"
#endif

#if __cplusplus >= 201100
#include <cstdint>
#else
#include <stdint.h>
#endif
#include <cstddef>
#include <cassert>

#ifdef __cpp_concepts
#include <concepts>
#include <type_traits>
#endif

#ifdef __c28x__
#include <emb/c28x.hpp>
#endif

#define EMB_UNUSED(arg) (void)arg;

#define EMB_UNIQ_ID_IMPL(line) _a_local_var_##line
#define EMB_UNIQ_ID(line) EMB_UNIQ_ID_IMPL(line)

#if __cplusplus >= 201100
#define EMB_OVERRIDE override
#define EMB_DEFAULT = default;
#else
#define EMB_OVERRIDE
#define EMB_DEFAULT {}
#endif

#if __cpp_constexpr >= 201304
#define EMB_INLINE_CONSTEXPR constexpr
#define EMB_CONSTEXPR constexpr
#else
#define EMB_INLINE_CONSTEXPR inline
#define EMB_CONSTEXPR
#endif

#if __cplusplus >= 201700
#define EMB_MAYBE_UNUSED [[maybe_unused]]
#else
#define EMB_MAYBE_UNUSED
#endif

#ifdef __cpp_static_assert
#define EMB_STATIC_ASSERT(cond) static_assert(cond)
#else
#define EMB_CAT_(a, b) a ## b
#define EMB_CAT(a, b) EMB_CAT_(a, b)
#define EMB_STATIC_ASSERT(cond) typedef int EMB_CAT(assert, __LINE__)[(cond) ? 1 : -1]
#endif

#ifndef __c28x__
#define EMB_STRINGIZE_IMPL(x) #x
#define EMB_STRINGIZE(x) EMB_STRINGIZE_IMPL(x)
#endif

#ifndef __c28x__
namespace emb {

void fatal_error_cb(const char* hint, int code);

[[noreturn]] inline void fatal_error(const char* hint, int code = 0) {
    fatal_error_cb(hint, code);
    while (true) {}
}

inline void empty_function() {
    // do nothing
}


[[noreturn]] inline void invalid_function() {
    fatal_error("invalid function call");
    while (true) {}
}

} // namespace emb

#if __cplusplus >= 201100

namespace emb {

// Primary template - type not found case
template<typename T, typename... Ts>
struct type_index;

// Base case: type found at position 0
template<typename T, typename... Ts>
struct type_index<T, T, Ts...> : std::integral_constant<std::size_t, 0> {};

// Recursive case: increment index and continue searching
template<typename T, typename U, typename... Ts>
struct type_index<T, U, Ts...>
    : std::integral_constant<std::size_t, 1 + type_index<T, Ts...>::value> {};

// Helper template
template<typename T, typename... Ts>
inline constexpr std::size_t type_index_v = type_index<T, Ts...>::value;

} // namespace emb

#endif

#ifdef __cpp_concepts

namespace emb {

template<typename T, typename... Ts>
concept one_of = std::disjunction_v<std::is_same<T, Ts>...>;
} // namespace emb

#endif

#endif
