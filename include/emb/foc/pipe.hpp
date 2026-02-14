#pragma once

#include <concepts>
#include <functional>
#include <type_traits>

namespace emb {
namespace foc {

template<typename T, typename Function>
  requires(std::invocable<Function, T>)
constexpr auto operator|(T&& t, Function&& f)
    -> std::invoke_result_t<Function, T> {
  return std::invoke(std::forward<Function>(f), std::forward<T>(t));
}

} // namespace foc
} // namespace emb
