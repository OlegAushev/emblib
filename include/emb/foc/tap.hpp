#pragma once

#include <type_traits>

namespace emb {
namespace foc {

template<typename F>
struct tap_fn {
  F func;

  template<typename T>
  constexpr T operator()(T&& value) const {
    func(value);
    return std::forward<T>(value);
  }
};

template<typename F>
constexpr tap_fn<std::decay_t<F>> tap(F&& f) {
  return {std::forward<F>(f)};
}

} // namespace foc
} // namespace emb
