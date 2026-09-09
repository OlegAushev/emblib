#pragma once

#include <emb/units.hpp>

namespace emb::sensor {

// A line through the origin, given one rated point:
//   y = rated_output * x / rated_input
// Only the linear characteristic is modelled: saturation beyond the rated
// range, offset and its drift are not.
template<emb::units::some_unit In, emb::units::some_unit Out>
struct proportional {
  In rated_input;
  Out rated_output;

  constexpr proportional(In rated_in, Out rated_out)
      : rated_input(rated_in), rated_output(rated_out)
  {
  }

  constexpr Out forward(In in) const
  {
    auto const out_per_in = rated_output.value() / rated_input.value();
    return Out{out_per_in * in.value()};
  }

  constexpr In inverse(Out out) const
  {
    auto const in_per_out = rated_input.value() / rated_output.value();
    return In{in_per_out * out.value()};
  }
};

} // namespace emb::sensor
