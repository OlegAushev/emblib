#pragma once

#include <expected>
#include <utility>

#ifndef __GNUC__
#error "emb/expected.hpp requires GNU statement expressions (GCC/Clang)"
#endif

// Early-return error propagation for std::expected:
//
//   auto load() -> std::expected<config, error> {
//     auto ver = TRY(read_version()); // ver is the unwrapped value
//     ...
//   }
//
// On error, returns std::unexpected(...) from the enclosing function, whose
// error type must be constructible from the operand's error type. An
// expected<void, E> operand is allowed; TRY(...) then yields void. An lvalue
// operand is copied from, an rvalue operand is moved from.
#define TRY(...)                                                               \
  __extension__ ({                                                             \
    auto&& emb_try_result_ = (__VA_ARGS__);                                    \
    if (!emb_try_result_.has_value()) [[unlikely]] {                           \
      return std::unexpected(                                                  \
          std::forward<decltype(emb_try_result_)>(emb_try_result_).error()     \
      );                                                                       \
    }                                                                          \
    *std::forward<decltype(emb_try_result_)>(emb_try_result_);                 \
  })
