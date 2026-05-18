#pragma once

#include <cassert>

#ifdef NDEBUG
#define ASSUME(expr) [[assume(expr)]]
#else
#define ASSUME(expr) assert(expr)
#endif