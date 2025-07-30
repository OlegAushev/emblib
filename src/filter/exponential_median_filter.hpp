#pragma once

#include <algorithm>
#include <emb/algorithm.hpp>
#include <emb/array.hpp>
#include <emb/circular_buffer.hpp>
#include <emb/core.hpp>

namespace emb {

template<typename T, size_t WindowSize>
class exponential_median_filter {
private:
  emb::circular_buffer<T, WindowSize> window_;
  float sampling_period_;
  float time_constant_;
  float smooth_factor_;
  T init_output_;
  T output_;
public:
  exponential_median_filter(
      float sampling_period,
      float time_constant,
      T const& init_output = T())
      : init_output_(init_output) {
    EMB_STATIC_ASSERT((WindowSize % 2) == 1);
    configure(sampling_period, time_constant);
    reset();
  }

  void push(T const& input_v) {
    window_.push_back(input_v);
    emb::array<T, WindowSize> window_sorted = {};
    std::copy(window_.begin(), window_.end(), window_sorted.begin());
    std::sort(window_sorted.begin(), window_sorted.end());
    T const median_v = window_sorted[WindowSize / 2];

    output_ = output_ + smooth_factor_ * (median_v - output_);
  }

  T output() const { return output_; }

  void set_output(T const& output_v) {
    window_.fill(output_v);
    output_ = output_v;
  }

  void reset() { set_output(init_output_); }

  void configure(float sampling_period, float time_constant) {
    sampling_period_ = sampling_period;
    time_constant_ = time_constant;
    smooth_factor_ = emb::clamp(sampling_period / time_constant, 0.0f, 1.0f);
  }

  void set_sampling_period(float ts) {
    sampling_period_ = ts;
    smooth_factor_ = emb::clamp(sampling_period_ / time_constant_, 0.0f, 1.0f);
  }

  float smooth_factor() const { return smooth_factor_; }
};

} // namespace emb
