#pragma once

#include <cassert>
#include <exception>

#ifdef NDEBUG
#define ASSUME(expr) [[assume(expr)]]
#else
#define ASSUME(expr) assert(expr)
#endif

namespace emb {

// Runtime invariant check. Calls std::terminate() if the predicate is false
// at runtime; falls back to assert() in constant evaluation. The linking
// project should install a custom terminate handler via std::set_terminate
// to perform an appropriate halt (e.g. breakpoint + spin, watchdog reset).
constexpr void ensure(bool pred) {
  if !consteval {
    if (!pred) {
      std::terminate();
    }
  } else {
    assert(pred);
  }
}

} // namespace emb
