#pragma once

#include <cstddef>
#include <tuple>
#include <utility>

namespace emb {

// Applies `f` to the element of `t` with the run-time index `i` and returns
// the result. If `t[i]` is a valid expression, e.g. for a `std::array` or a
// built-in array, equivalent to `f(t[i])`; for an array, the behavior is
// undefined if `i` is out of range. Otherwise, for a tuple such as
// `std::tuple` or `std::pair`, `i` is compared with the element indices in
// turn, and `f` is applied to the element whose index equals `i`, or to the
// last element if `i` is out of range. `f` must accept every element, and
// its results are brought to a common type as if by the conditional
// operator, e.g. `double` for an `int` and a `double`. The program is
// ill-formed if there is no common type, e.g. for `void` and `int`, or if
// the tuple is empty. The template parameter `I` is internal and is left at
// its default.
// TODO(C++26 expansion statements, GCC 16): template for
template<std::size_t I = 0, typename Tuple, typename F>
constexpr decltype(auto) visit_at(Tuple& t, std::size_t i, F&& f)
{
  if constexpr (requires { t[i]; }) {
    return f(t[i]);
  }
  else if constexpr (I + 1 == std::tuple_size_v<Tuple>) {
    return f(std::get<I>(t));
  }
  else {
    return i == I ? f(std::get<I>(t)) : visit_at<I + 1>(t, i, f);
  }
}

} // namespace emb
