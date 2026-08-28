#pragma once

#include <cstddef>
#include <tuple>
#include <utility>

namespace emb {

// Applies f to the i-th element of a tuple-like, i known only at run time.
// A container with operator[] (std::array) is indexed directly; a tuple goes
// through a compare chain that the compiler folds into a jump table, so f
// must accept every element type. i must be below the element count: the
// last branch is unconditional.
// TODO(C++26 expansion statements, GCC 16): template for
template<std::size_t I = 0, typename Tuple, typename F>
constexpr decltype(auto) visit_at(Tuple& t, std::size_t i, F&& f) {
  if constexpr (requires { t[i]; }) {
    return f(t[i]);
  } else if constexpr (I + 1 == std::tuple_size_v<Tuple>) {
    return f(std::get<I>(t));
  } else {
    return i == I ? f(std::get<I>(t)) : visit_at<I + 1>(t, i, f);
  }
}

} // namespace emb
