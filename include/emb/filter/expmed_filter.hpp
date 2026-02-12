#pragma once

#include <emb/algorithm.hpp>
#include <emb/circular_buffer.hpp>
#include <emb/core.hpp>

#include <algorithm>
#include <array>

namespace emb {
namespace filter {

template<typename T, size_t WindowSize, typename Duration>
class exponential_median {
public:
  using value_type = T;
  using duration_type = Duration;
  using factor_type =
      decltype(std::declval<Duration>() / std::declval<Duration>());
private:
  emb::circular_buffer<value_type, WindowSize> window_;
  duration_type sampling_period_;
  duration_type time_constant_;
  factor_type smooth_factor_;
  value_type init_output_;
  value_type output_;
public:
  exponential_median(
      duration_type sampling_period,
      duration_type time_constant,
      value_type const& init_output = value_type()
  )
      : init_output_(init_output) {
    EMB_STATIC_ASSERT((WindowSize % 2) == 1);
    configure(sampling_period, time_constant);
    reset();
  }

  void push(value_type const& input_v) {
    window_.push_back(input_v);
    std::array<value_type, WindowSize> window_sorted = {};
    std::copy(window_.begin(), window_.end(), window_sorted.begin());
    std::sort(window_sorted.begin(), window_sorted.end());
    T const median_v = window_sorted[WindowSize / 2];

    output_ = output_ + smooth_factor_ * (median_v - output_);
  }

  value_type output() const {
    return output_;
  }

  void set_output(value_type const& output_v) {
    window_.fill(output_v);
    output_ = output_v;
  }

  void reset() {
    set_output(init_output_);
  }

  void configure(duration_type sampling_period, duration_type time_constant) {
    sampling_period_ = sampling_period;
    time_constant_ = time_constant;
    smooth_factor_ = std::clamp(
        sampling_period / time_constant,
        factor_type(0),
        factor_type(1)
    );
  }

  void set_sampling_period(duration_type ts) {
    sampling_period_ = ts;
    smooth_factor_ = std::clamp(
        sampling_period_ / time_constant_,
        factor_type(0),
        factor_type(1)
    );
  }

  factor_type smooth_factor() const {
    return smooth_factor_;
  }
};

} // namespace filter
} // namespace emb
