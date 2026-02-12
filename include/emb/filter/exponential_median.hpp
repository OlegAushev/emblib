#pragma once

#include <emb/circular_buffer.hpp>
#include <emb/math.hpp>

#include <algorithm>
#include <array>

namespace emb {
namespace filter {

template<typename T, size_t WindowSize, typename Duration>
  requires(emb::isodd(WindowSize))
class exponential_median {
public:
  using value_type = T;
  using reference = value_type&;
  using const_reference = value_type const&;
  using duration_type = Duration;
  using factor_type =
      decltype(std::declval<Duration>() / std::declval<Duration>());
  static constexpr size_t window_size = WindowSize;
private:
  emb::circular_buffer<value_type, window_size> window_;
  duration_type sampling_period_;
  duration_type time_constant_;
  factor_type smooth_factor_;
  value_type init_output_;
  value_type output_;
public:
  constexpr exponential_median(
      duration_type sampling_period,
      duration_type time_constant,
      value_type const& init_output = value_type()
  )
      : init_output_(init_output) {
    configure(sampling_period, time_constant);
    reset();
  }

  constexpr void push(value_type const& input_v) {
    window_.push_back(input_v);
    std::array<value_type, window_size> window_sorted = {};
    std::copy(window_.begin(), window_.end(), window_sorted.begin());
    std::sort(window_sorted.begin(), window_sorted.end());
    value_type const median_v = window_sorted[window_size / 2];

    output_ = output_ + smooth_factor_ * (median_v - output_);
  }

  constexpr const_reference output() const {
    return output_;
  }

  constexpr void set_output(value_type const& output_v) {
    window_.fill(output_v);
    output_ = output_v;
  }

  constexpr void reset() {
    set_output(init_output_);
  }

  constexpr void configure(duration_type sampling_period, duration_type time_constant) {
    sampling_period_ = sampling_period;
    time_constant_ = time_constant;
    smooth_factor_ = std::clamp(
        sampling_period / time_constant,
        factor_type(0),
        factor_type(1)
    );
  }

  constexpr void set_sampling_period(duration_type ts) {
    sampling_period_ = ts;
    smooth_factor_ = std::clamp(
        sampling_period_ / time_constant_,
        factor_type(0),
        factor_type(1)
    );
  }

  constexpr factor_type smooth_factor() const {
    return smooth_factor_;
  }
};

} // namespace filter
} // namespace emb
