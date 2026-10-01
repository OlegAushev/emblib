#pragma once

#include <tuple>

namespace emb::signal {

namespace detail {

// Returns the result of applying the `forward` of each given stage to `x` in
// turn, first to last, or `x` if no stage is given.
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

// Returns the result of applying the `inverse` of each given stage to `y` in
// turn, last to first, or `y` if no stage is given.
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

// The class template `path` models a physical signal path as a composition of
// the invertible stages `Stages`, listed in the forward direction, i.e. from a
// physical quantity to a raw code such as an ADC code. A stage is a class with
// `const` or static member functions `forward` and `inverse` that undo each
// other: `forward` converts the input of the stage to its output, and
// `inverse` converts the output back to the input.
//
// `forward` converts a physical quantity to a raw code by applying the
// `forward` of each stage, first to last; this is the driving direction, also
// used to synthesize codes such as test vectors and thresholds. `inverse`
// converts a raw code to a physical quantity by applying the `inverse` of each
// stage, last to first, and so undoes `forward`; this is the measuring
// direction. Neither direction is the default, so a `path` is not callable.
//
// The stage objects are kept in the public tuple `stages`, so that a stage
// that carries run-time state, such as a zero trim, can be read and changed.
template<typename... Stages>
class path {
public:
  std::tuple<Stages...> stages;

  constexpr path() = default;

  constexpr explicit path(Stages... s) : stages{s...} {}

  constexpr auto forward(auto in) const
  {
    return std::apply(
        [&](auto const&... s) { return detail::path_forward(in, s...); },
        stages);
  }

  constexpr auto inverse(auto out) const
  {
    return std::apply(
        [&](auto const&... s) { return detail::path_inverse(out, s...); },
        stages);
  }
};

} // namespace emb::signal
