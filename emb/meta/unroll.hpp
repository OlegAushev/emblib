#pragma once

#include <cstddef>
#include <utility>

namespace emb {

// Calls `f.template operator()<I>()` for each `I` in [0, `N`), in increasing
// order of `I`, and discards the values the calls return. `f` can be a lambda
// `[&]<std::size_t I>() { ... }`. If `N` is zero, there are no effects.
template<std::size_t N, typename F>
constexpr void unroll(F&& f)
{
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    ((void)f.template operator()<Is>(), ...);
  }(std::make_index_sequence<N>{});
}

} // namespace emb
