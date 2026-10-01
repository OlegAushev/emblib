#pragma once

#include <emb/units.hpp>

namespace emb::signal {

// The class template `proportional` is a stage whose characteristic is a line
// through the origin, given by one rated point: the output `rated_output` at
// the input `rated_input`. Up to rounding, `forward(in)` returns
// `rated_output * (in / rated_input)`, and `inverse(out)` returns
// `rated_input * (out / rated_output)`. `In` and `Out` may be the same unit,
// e.g. for a current transducer with a current output.
//
// `proportional` models only the linear characteristic, with no saturation
// beyond the rated range and no offset or drift. The behavior of `forward` is
// undefined if `rated_input` is zero, and that of `inverse` if `rated_output`
// is zero.
//
// `proportional` is a structural type, so a `proportional` can be the template
// argument of `bind`.
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
    auto const out_per_in = rated_output.value / rated_input.value;
    return Out{out_per_in * in.value};
  }

  constexpr In inverse(Out out) const
  {
    auto const in_per_out = rated_input.value / rated_output.value;
    return In{in_per_out * out.value};
  }
};

} // namespace emb::signal
