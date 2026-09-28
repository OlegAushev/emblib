#pragma once

#include <emb/container/circular_buffer.hpp>
#include <emb/math.hpp>
#include <emb/units.hpp>

#include <algorithm>
#include <array>
#include <concepts>
#include <utility>

namespace emb {

// The class template `exponential_median_filter` is a filter whose output is an
// exponential moving average of the median of the last `WindowSize` values
// pushed: `push` moves the output toward the new median by the smoothing factor
// times their difference. Construction and `reset()` set the output and every
// value in the window to the initial output, and `set_output(value)` to
// `value`; a finite output then keeps its value for the next `WindowSize / 2`
// pushes.
//
// The smoothing factor is the sampling period divided by the time constant,
// clamped to [0, 1]; `set_smoothing` sets both durations and `set_timestep`
// only the sampling period. For a sampling period much shorter than the time
// constant, the smoothing approximates a first-order low-pass filter with a
// time constant about half a sampling period shorter; if the sampling period
// is at least the time constant, `push` sets the output to the new median, up
// to rounding. The time constant must be positive; the behavior is undefined
// if the quotient is NaN.
//
// The values in the window must not be NaN: with a NaN and two unequal numbers
// in it, `<` is not the strict weak ordering that `std::sort` requires, and the
// behavior is undefined. Each `push` sorts a local copy of the window, which
// takes O(N log N) comparisons, where N is `WindowSize`.
template<typename T, std::size_t WindowSize, typename Duration>
  requires(emb::isodd(WindowSize))
       && (std::floating_point<T> || emb::units::some_unit<T>)
       && std::floating_point<decltype(std::declval<Duration>()
                                       / std::declval<Duration>())>
class exponential_median_filter {
public:
  using value_type = T;
  using duration_type = Duration;
  using factor_type =
      decltype(std::declval<Duration>() / std::declval<Duration>());
  static constexpr std::size_t window_size = WindowSize;
private:
  emb::circular_buffer<value_type, window_size> window_;
  duration_type sampling_period_;
  duration_type time_constant_;
  factor_type smooth_factor_;
  value_type init_output_;
  value_type output_;
public:
  constexpr exponential_median_filter(duration_type sampling_period,
                                      duration_type time_constant,
                                      value_type init_output = value_type())
      : init_output_(init_output)
  {
    set_smoothing(sampling_period, time_constant);
    reset();
  }

  constexpr void push(value_type input)
  {
    window_.push_back(input);
    std::array<value_type, window_size> window_sorted = {};
    for (auto i = 0uz; i < window_.size(); ++i) {
      window_sorted[i] = window_[i];
    }
    std::sort(window_sorted.begin(), window_sorted.end());
    value_type const median = window_sorted[window_size / 2];

    output_ = output_ + smooth_factor_ * (median - output_);
  }

  constexpr value_type output() const
  {
    return output_;
  }

  constexpr void set_output(value_type value)
  {
    window_.fill(value);
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
