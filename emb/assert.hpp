#pragma once

#include <cassert>
#include <exception>

#include <emb/meta/fixed_string.hpp>

namespace emb {

// Terminates the program by calling `std::terminate`. A call to `panic` marks
// a point that a correct program does not reach, typically the branch of a
// failed check: `if (index >= size) emb::panic();`. Unlike `EMB_PRE`, such a
// check is made at run time whether or not `NDEBUG` is defined.
//
// `panic` is not `constexpr`, so a call to it reached during constant
// evaluation makes the program ill-formed in a context that requires a
// constant expression, e.g. a `constexpr` initializer. Elsewhere, e.g. in the
// initializer of a variable that is not `constexpr`, the call is evaluated at
// run time and terminates.
//
// The effect at run time is that of the terminate handler installed with
// `std::set_terminate`, or `std::abort` if none is installed; a program
// installs one to halt the device as appropriate (e.g. breakpoint and spin,
// watchdog reset).
[[noreturn]] inline void panic() noexcept
{
  std::terminate();
}

// Terminates like `panic()`, but a call to it reached during constant
// evaluation names `Message` in the diagnostic, as the template argument of a
// function that is not `constexpr`. A program calls it through `EMB_PANIC`.
template<fixed_string Message>
[[noreturn]] void panic() noexcept
{
  std::terminate();
}

// Checks `pred`: `ensure(pred)` is `if (!pred) panic();`.
//
// Deprecated: write `if (!pred) emb::panic();`, which names the condition of
// the failure instead of the one that holds.
[[deprecated("write `if (!pred) emb::panic();`")]]
constexpr void ensure(bool pred)
{
  if (!pred) panic();
}

} // namespace emb

// The macro `EMB_PANIC` marks a point that a correct program does not reach,
// for the reason `message`, a string literal. In a context that requires a
// constant expression, reaching it makes the program ill-formed, and the
// diagnostic shows `message`. Elsewhere, e.g. in a `constexpr` function called
// at run time or in the initializer of a variable that is not `constexpr`, it
// calls `std::terminate`. GCC shows a character of `message` outside ASCII as
// an octal escape.
//
// `EMB_PANIC(message)` is an expression of type `void`.
#define EMB_PANIC(message) ::emb::panic<message>()

// The macro `EMB_PRE` states a precondition `cond` that the caller of a
// function guarantees, e.g. that a container is not empty when its `front()`
// is called. It is written at the start of the function's body, where C++26
// would put `pre(cond)` on the declaration.
//
// If `NDEBUG` is not defined, `cond` is evaluated, and if it is `false`,
// `EMB_PRE` fails as `EMB_PANIC` does, with the reason `"pre: "` followed by
// the text of `cond`, e.g. `"pre: size_ != 0"`. If `NDEBUG` is defined, `cond`
// is checked only during constant evaluation: if it is `false` in a context
// that requires a constant expression, the program is ill-formed as above, and
// elsewhere the failure is not detected. At run time `EMB_PRE` adds no code,
// and a call that breaks the precondition behaves as if `EMB_PRE` were absent.
//
// `EMB_PRE(cond);` can be used only as a statement. `cond` may contain commas,
// e.g. `EMB_PRE(std::is_same_v<T, U>)`, and must have no side effects, since
// whether it is evaluated depends on `NDEBUG`.
#ifdef NDEBUG
#define EMB_PRE(...)                                                         \
  do {                                                                       \
    if consteval {                                                           \
      if (!(__VA_ARGS__)) EMB_PANIC("pre: " #__VA_ARGS__);                   \
    }                                                                        \
  } while (false)
#else
#define EMB_PRE(...)                                                         \
  do {                                                                       \
    if (!(__VA_ARGS__)) EMB_PANIC("pre: " #__VA_ARGS__);                     \
  } while (false)
#endif
