#pragma once

#if !defined(EMBLIB_C28X) && !defined(EMBLIB_ARM) && !defined(EMBLIB_X86)
#error "emblib error: arch not defined!"
#endif

#if __cplusplus >= 201100
#include <cstdint>
#else
#include <stdint.h>
#endif
#include <cstddef>
#include <cassert>

#ifdef EMBLIB_C28X
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

#ifndef EMBLIB_C28X
#define EMB_STRINGIZE_IMPL(x) #x
#define EMB_STRINGIZE(x) EMB_STRINGIZE_IMPL(x)
#endif

#ifndef EMBLIB_C28X
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
#endif
