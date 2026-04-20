#pragma once

#include <functional>

namespace emb {

constexpr decltype(auto) curry(auto f, auto... ps) {
  if constexpr (requires { std::invoke(f, ps...); }) {
    return std::invoke(f, ps...);
  } else {
    return [f, ps...](auto... qs) -> decltype(auto) {
      return curry(f, ps..., qs...);
    };
  }
}

} // namespace emb
