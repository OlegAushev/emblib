#pragma once

#include <emb/units.hpp>

namespace emb::signal {

// A line through the ends of two ranges: (in_min, in_max) maps onto
// (out_min, out_max).
//
// Values outside the input range extrapolate; nothing here clamps them.
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
        (output_max - output_min).value() / (input_max - input_min).value();
    auto const offset = output_min.value() - out_per_in * input_min.value();
    return Out{out_per_in * in.value() + offset};
  }

  constexpr In inverse(Out out) const
  {
    auto const in_per_out =
        (input_max - input_min).value() / (output_max - output_min).value();
    auto const offset = input_min.value() - in_per_out * output_min.value();
    return In{in_per_out * out.value() + offset};
  }
};

} // namespace emb::signal
