#pragma once

#include <expected>
#include <utility>

#ifndef __GNUC__
#error "emb/expected.hpp requires GNU statement expressions (GCC/Clang)"
#endif

// The macro `TRY` yields the value held in a `std::expected`, or, if the
// `std::expected` holds an error, returns that error from the enclosing
// function. `TRY(expr)` evaluates `expr`, an expression of type
// `std::expected<T, E>`, exactly once. The error is returned as a
// `std::unexpected<E>`; if `T` is `void`, `TRY(expr)` is an expression of type
// `void`. The value or the error is copied from `expr` if `expr` is an lvalue
// and moved from it otherwise.
//
// `TRY` can be used only in the body of a function whose return type is not
// deduced and to which `std::unexpected<E>` is implicitly convertible, e.g.
// `std::expected<U, G>` where `E` is implicitly convertible to `G`. If
// `TRY(expr)` is part of a larger expression and returns, it is unspecified
// which other subexpressions of that expression have been evaluated, except
// those sequenced before or after it. If `expr` contains another `TRY`,
// `-Wshadow` reports that the variable `emb_try_result_` declared by the inner
// `TRY` shadows the one declared by the outer `TRY`; GCC reports it even when
// the inner `TRY` is in the body of a lambda.
//
// `TRY` can be used in a `constexpr` function, but with Clang 22, and with
// GCC 15 unless `T` is `void`, an evaluation in which `TRY(expr)` returns is
// not a constant expression. GCC 16 has no such limitation.
#define TRY(...)                                                             \
  __extension__({                                                            \
    auto&& emb_try_result_ = (__VA_ARGS__);                                  \
    if (!emb_try_result_.has_value()) [[unlikely]] {                         \
      return std::unexpected(                                                \
          std::forward<decltype(emb_try_result_)>(emb_try_result_).error()); \
    }                                                                        \
    *std::forward<decltype(emb_try_result_)>(emb_try_result_);               \
  })
