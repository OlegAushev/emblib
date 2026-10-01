#pragma once

#include <emb/units.hpp>

namespace emb::signal {

// The class template `affine` is a stage that maps an input range onto an
// output range along a straight line. `forward` maps `input_min` to
// `output_min` and `input_max` to `output_max`, up to floating-point rounding,
// and `inverse` converts the output back to the input. Outside the ranges, both
// extrapolate along the same line; neither clamps its result.
//
// `input_min` need not be less than `input_max`, nor `output_min` less than
// `output_max`: e.g. with an `output_min` of 20 mA and an `output_max` of 4 mA,
// the output falls as the input rises. The behavior of `forward` is undefined
// if `input_min` equals `input_max`, and that of `inverse` if `output_min`
// equals `output_max`.
//
// `affine` is a structural type, so an `affine` can be the template argument of
// `bind`.
template<emb::units::some_unit In, emb::units::some_unit Out>
struct affine {
  In input_min;
  In input_max;
  Out output_min;
  Out output_max;

  constexpr affine(In in_min, In in_max, Out out_min, Out out_max)
      : input_min(in_min),
        input_max(in_max),
        output_min(out_min),
        output_max(out_max)
  {
  }

  constexpr Out forward(In in) const
  {
    auto const out_per_in =
        (output_max - output_min).value / (input_max - input_min).value;
    auto const offset = output_min.value - out_per_in * input_min.value;
    return Out{out_per_in * in.value + offset};
  }

  constexpr In inverse(Out out) const
  {
    auto const in_per_out =
        (input_max - input_min).value / (output_max - output_min).value;
    auto const offset = input_min.value - in_per_out * output_min.value;
    return In{in_per_out * out.value + offset};
  }
};

} // namespace emb::signal
