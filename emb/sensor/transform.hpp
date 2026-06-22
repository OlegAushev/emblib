#pragma once

#include <tuple>

namespace emb::sensor {

// A transform is a composition of invertible stages describing the physical
// signal path in the *forward* direction (measured value -> raw code).
// Each stage exposes forward()/inverse(); the transform then provides:
//   forward(value) -> raw code  -- synthesize codes (test vectors, thresholds)
//   inverse(code)  -> value     -- the actual measurement / conversion
// operator() aliases inverse(), so a transform satisfies
// emb::sensor::some_converter and can be handed to a sensor
// (singlephase/polyphase/buffered) directly.

namespace detail {

template<typename X>
constexpr X transform_forward(X x) {
  return x;
}

template<typename X, typename Stage, typename... Rest>
constexpr auto transform_forward(X x, Stage const& s, Rest const&... rest) {
  return transform_forward(s.forward(x), rest...);
}

template<typename Y>
constexpr Y transform_inverse(Y y) {
  return y;
}

template<typename Y, typename Stage, typename... Rest>
constexpr auto transform_inverse(Y y, Stage const& s, Rest const&... rest) {
  return s.inverse(transform_inverse(y, rest...));
}

} // namespace detail

template<typename... Stages>
class transform {
public:
  std::tuple<Stages...> stages;

  constexpr explicit transform(Stages... s) : stages{s...} {}

  // measured value -> raw code (composes stages front to back)
  constexpr auto forward(auto value) const {
    return std::apply(
        [&](auto const&... s) {
          return detail::transform_forward(value, s...);
        },
        stages
    );
  }

  // raw code -> measured value (inverts stages back to front)
  constexpr auto inverse(auto code) const {
    return std::apply(
        [&](auto const&... s) { return detail::transform_inverse(code, s...); },
        stages
    );
  }

  // a transform is itself an emb::sensor::some_converter
  constexpr auto operator()(auto code) const {
    return inverse(code);
  }
};

} // namespace emb::sensor
