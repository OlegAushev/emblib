#pragma once

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

namespace emb {
namespace pipe {

template<typename T, typename Function>
  requires(std::invocable<Function, T>)
constexpr auto operator|(T&& t, Function&& f)
    -> std::invoke_result_t<Function, T> {
  return std::invoke(std::forward<Function>(f), std::forward<T>(t));
}

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

template<typename T>
class store_to {
  T& dest_;
public:
  constexpr explicit store_to(T& dest) : dest_(dest) {}

  constexpr T operator()(T const& value) const {
    dest_ = value;
    return value;
  }
};

} // namespace pipe
} // namespace emb
