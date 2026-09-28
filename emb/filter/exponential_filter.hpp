#pragma once

#include <emb/units.hpp>

#include <algorithm>
#include <concepts>
#include <utility>

namespace emb {

// The class template `exponential_filter` is a filter whose output is an
// exponential moving average of the values pushed: `push(input)` moves the
// output toward `input` by the smoothing factor times their difference.
// Construction and `reset()` set the output to the initial output, and
// `set_output(value)` to `value`.
//
// The smoothing factor is the sampling period divided by the time constant,
// clamped to [0, 1]; `set_smoothing` sets both durations and `set_timestep`
// only the sampling period. For a sampling period much shorter than the time
// constant, the filter approximates a first-order low-pass filter with a time
// constant about half a sampling period shorter; if the sampling period is at
// least the time constant, `push(input)` sets the output to `input`, up to
// rounding. The time constant must be positive; the behavior is undefined if
// the quotient is NaN.
template<typename T, typename Duration>
  requires(std::floating_point<T> || emb::units::some_unit<T>)
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
  duration_type time_constant_;
  factor_type smooth_factor_;
  value_type init_output_;
  value_type output_;
public:
  constexpr exponential_filter(duration_type sampling_period,
                               duration_type time_constant,
                               value_type init_output = value_type())
      : init_output_(init_output)
  {
    set_smoothing(sampling_period, time_constant);
    reset();
  }

  constexpr void push(value_type input)
  {
    output_ = output_ + smooth_factor_ * (input - output_);
  }

  constexpr value_type output() const
  {
    return output_;
  }

  constexpr void set_output(value_type value)
  {
    output_ = value;
  }

  constexpr void reset()
  {
    set_output(init_output_);
  }

  constexpr void set_smoothing(duration_type sampling_period,
                               duration_type time_constant)
  {
    sampling_period_ = sampling_period;
    time_constant_ = time_constant;
    smooth_factor_ = std::clamp(sampling_period / time_constant,
                                factor_type(0),
                                factor_type(1));
  }

  constexpr void set_timestep(duration_type ts)
  {
    sampling_period_ = ts;
    smooth_factor_ = std::clamp(sampling_period_ / time_constant_,
                                factor_type(0),
                                factor_type(1));
  }

  constexpr factor_type smooth_factor() const
  {
    return smooth_factor_;
  }
};

} // namespace emb
