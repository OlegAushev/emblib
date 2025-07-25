#pragma once

#include <algorithm>
#include <emblib/algorithm.hpp>
#include <emblib/core.hpp>

namespace emb {

template<typename T>
class exponential_filter {
private:
  float sampling_period_;
  float time_constant_;
  float smooth_factor_;
  T init_output_;
  T output_;
public:
  exponential_filter(
      float sampling_period,
      float time_constant,
      T const& init_output = T())
      : init_output_(init_output) {
    configure(sampling_period, time_constant);
    reset();
  }

  void push(T const& input_v) {
    output_ = output_ + smooth_factor_ * (input_v - output_);
  }

  T output() const { return output_; }

  void set_output(T const& output_v) { output_ = output_v; }

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
