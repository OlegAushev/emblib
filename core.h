#pragma once


#if !defined(EMBLIB_C28X) && !defined(EMBLIB_STM32)
#error "emblib error: arch not defined!"
#endif


#if defined(EMBLIB_C28X)
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#elif defined(EMBLIB_STM32)
#include <cstdint>
#include <cstddef>
#include <cassert>
#endif


#if defined(EMBLIB_C28X)
#include "core/c28x.h"
#include "core/scopedenum.h"
#endif

#include "core/interrupt_invoker.h"
#include "core/monostate.h"
#include "core/noncopyable.h"


#define EMB_UNUSED(arg) (void)arg;


#define EMB_UNIQ_ID_IMPL(line) _a_local_var_##line
#define EMB_UNIQ_ID(line) EMB_UNIQ_ID_IMPL(line)


#if defined(EMBLIB_C28X)

#define EMB_OVERRIDE
#define EMB_DEFAULT {}
#define EMB_MAYBE_UNUSED

#elif defined(EMBLIB_STM32)

#define EMB_OVERRIDE override
#define EMB_DEFAULT = default;
#define EMB_MAYBE_UNUSED [[maybe_unused]] 

#endif


#if defined(EMBLIB_C28X)

#define EMB_CAT_(a, b) a ## b
#define EMB_CAT(a, b) EMB_CAT_(a, b)
#define EMB_STATIC_ASSERT(cond) typedef int EMB_CAT(assert, __LINE__)[(cond) ? 1 : -1]
#define EMB_MILLISECONDS emb::chrono::milliseconds

#elif defined(EMBLIB_STM32)

#define EMB_STATIC_ASSERT(cond) static_assert(cond)
#define EMB_MILLISECONDS std::chrono::milliseconds

#endif


#if defined(EMBLIB_STM32)

#define EMB_STRINGIZE_IMPL(x) #x
#define EMB_STRINGIZE(x) EMB_STRINGIZE_IMPL(x)

#endif


#if defined(EMBLIB_STM32)

namespace emb {


void fatal_error_cb(const char* hint, int code);


[[ noreturn ]] inline void fatal_error(const char* hint, int code = 0)
{
    fatal_error_cb(hint, code);
    while (true) {}
}


inline void empty_function()
{
    /* DO NOTHING */
}


[[ noreturn ]] inline void invalid_function()
{
    fatal_error("invalid function call");
    while (true) {}
}


} // namespace emb

#endif
