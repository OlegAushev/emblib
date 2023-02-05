#pragma once


#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#include <c28x_emb/emb_core/emb_c28x.h>
#include <c28x_emb/emb_core/emb_monostate.h>
#include <c28x_emb/emb_core/emb_noncopyable.h>
#include <c28x_emb/emb_core/emb_scopedenum.h>


#define EMB_UNUSED(arg) (void)arg;


#define EMB_UNIQ_ID_IMPL(line) _a_local_var_##line
#define EMB_UNIQ_ID(line) EMB_UNIQ_ID_IMPL(line)


#define EMB_CAT_(a, b) a ## b
#define EMB_CAT(a, b) EMB_CAT_(a, b)
#define EMB_STATIC_ASSERT(cond) typedef int EMB_CAT(assert, __LINE__)[(cond) ? 1 : -1]

