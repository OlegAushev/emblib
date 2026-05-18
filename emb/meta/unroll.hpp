#pragma once

#include <cstddef>
#include <utility>

namespace emb {

template<size_t N, typename F>
constexpr void unroll(F&& f) {
  [&]<std::size_t... Is>(std::index_sequence<Is...>) {
    (f.template operator()<Is>(), ...);
  }(std::make_index_sequence<N>{});
}

} // namespace emb
