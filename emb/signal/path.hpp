#pragma once

#include <tuple>

namespace emb::signal {

// A path is a composition of invertible stages describing a physical signal
// path in the *forward* direction (physical quantity -> raw code).
// Each stage exposes forward()/inverse(); the path then provides:
//   forward(value) -> raw code  -- the driving direction, and code synthesis
//                                  (test vectors, thresholds)
//   inverse(code)  -> value     -- the measuring direction
// A path is not itself callable. Which of the two directions a bare call
// means is the caller's convention, not the model's: a sensor calls the
// measuring direction, a continuous actuator the driving one. Whoever adapts
// a path to a plain callable states which, and does it in its own namespace.

namespace detail {

template<typename X>
constexpr X path_forward(X x)
{
  return x;
}

template<typename X, typename Stage, typename... Rest>
constexpr auto path_forward(X x, Stage const& s, Rest const&... rest)
{
  return path_forward(s.forward(x), rest...);
}

template<typename Y>
constexpr Y path_inverse(Y y)
{
  return y;
}

template<typename Y, typename Stage, typename... Rest>
constexpr auto path_inverse(Y y, Stage const& s, Rest const&... rest)
{
  return s.inverse(path_inverse(y, rest...));
}

} // namespace detail

template<typename... Stages>
class path {
public:
  std::tuple<Stages...> stages;

  constexpr path() = default;

  constexpr explicit path(Stages... s) : stages{s...} {}

  // measured quantity -> raw code
  // composes stages front to back
  constexpr auto forward(auto in) const
  {
    return std::apply(
        [&](auto const&... s) { return detail::path_forward(in, s...); },
        stages);
  }

  // raw code -> measured quantity
  // inverts stages back to front
  constexpr auto inverse(auto out) const
  {
    return std::apply(
        [&](auto const&... s) { return detail::path_inverse(out, s...); },
        stages);
  }
};

} // namespace emb::signal
