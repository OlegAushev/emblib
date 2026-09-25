#pragma once

#include <cassert>
#include <exception>

// The macro `ASSUME` states a precondition or invariant `expr` that the
// caller guarantees, e.g. that a container is not empty when its `front()` is
// called. If `NDEBUG` is not defined, `ASSUME(expr)` expands to
// `assert(expr)`: `expr` is evaluated and the program is aborted if it is
// `false`. Otherwise it expands to `[[assume(expr)]]`: `expr` is not
// evaluated, the compiler may optimize on the assumption that it is `true`,
// and the behavior is undefined if it would be `false`.
//
// `ASSUME(expr);` can be used only as a statement. `expr` must have no side
// effects, since whether it is evaluated depends on `NDEBUG`.
#ifdef NDEBUG
#define ASSUME(expr) [[assume(expr)]]
#else
#define ASSUME(expr) assert(expr)
#endif

namespace emb {

// Handles a failed `ensure` check by calling `std::terminate`. The effect at
// run time is that of the terminate handler installed with
// `std::set_terminate`, or `std::abort` if none is installed.
//
// `ensure_failed` is not `constexpr`, so a call to it reached during constant
// evaluation, e.g. `ensure(false)` in a `constexpr` initializer, makes the
// program ill-formed.
[[noreturn]] inline void ensure_failed() noexcept
{
  std::terminate();
}

// Checks the invariant `pred`: if `pred` is `false`, calls `ensure_failed()`,
// which calls `std::terminate`; otherwise there are no effects. Unlike
// `ASSUME`, the check is made whether or not `NDEBUG` is defined.
//
// In a context that requires a constant expression, a `false` `pred` makes
// the program ill-formed. Elsewhere, e.g. in the initializer of a variable
// that is not `constexpr`, the call is evaluated at run time and terminates.
//
// The effect of a failed check is that of the terminate handler; a program
// installs one with `std::set_terminate` to halt the device as appropriate
// (e.g. breakpoint and spin, watchdog reset).
constexpr void ensure(bool pred)
{
  if (!pred) {
    ensure_failed();
  }
}

} // namespace emb
