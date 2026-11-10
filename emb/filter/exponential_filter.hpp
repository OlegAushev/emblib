#pragma once

#include <emb/units.hpp>

#include <algorithm>
#include <concepts>
#include <utility>

namespace emb {

// The class template `exponential_filter` is a filter whose output is an
// exponential moving average of its inputs, one per sampling period:
// `step(input)` moves the output toward `input` by the smoothing factor times
// their difference. Construction sets the output to the initial output, and
// `reset(value)` to `value`.
//
// The smoothing factor is the sampling period divided by the time constant,
// clamped to [0, 1]. `set_time_constant` replaces the time constant; the
// sampling period stays the one given at construction. For a sampling period
// much shorter than the time constant, the filter approximates a first-order
// low-pass filter with a time constant about half a sampling period shorter;
// if the sampling period is at least the time constant, `step(input)` sets the
// output to `input`, up to rounding. The time constant must be positive; the
// behavior is undefined if the quotient is NaN.
template<typename T, typename Duration>
  requires(std::floating_point<T> || emb::units::quantity_specialization<T>)
       && std::floating_point<decltype(std::declval<Duration>()
                                       / std::declval<Duration>())>
class exponential_filter {
public:
  using value_type = T;
  using duration_type = Duration;
  using factor_type =
      decltype(std::declval<Duration>() / std::declval<Duration>());
private:
  duration_type sampling_period_;
  factor_type smooth_factor_;
  value_type output_;
public:
  constexpr exponential_filter(duration_type sampling_period,
                               duration_type time_constant,
                               value_type init_output = value_type{})
      : sampling_period_(sampling_period), output_(init_output)
  {
    set_time_constant(time_constant);
  }

  constexpr void step(value_type input)
  {
    output_ = output_ + smooth_factor_ * (input - output_);
  }

  constexpr value_type output() const
  {
    return output_;
  }

  constexpr void reset(value_type value)
  {
    output_ = value;
  }

  constexpr void set_time_constant(duration_type value)
  {
    smooth_factor_ =
        std::clamp(sampling_period_ / value, factor_type(0), factor_type(1));
  }

  constexpr factor_type smooth_factor() const
  {
    return smooth_factor_;
  }

  friend constexpr void filter_feed(exponential_filter& f, value_type input)
  {
    f.step(input);
  }

  friend constexpr value_type filter_output(exponential_filter const& f)
  {
    return f.output();
  }
};

} // namespace emb
