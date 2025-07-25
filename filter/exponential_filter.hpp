#pragma once

#include <algorithm>
#include <emblib/algorithm.hpp>
#include <emblib/array.hpp>
#include <emblib/circular_buffer.hpp>
#include <emblib/core.hpp>
#include <emblib/noncopyable.hpp>
#include <float.h>

namespace emb {

template<typename T>
class exponential_filter {
private:
  float sampling_period_;
  float time_constant_;
  float smooth_factor_;
  T out_;
public:
  exponential_filter()
      : sampling_period_(0), time_constant_(FLT_MAX), smooth_factor_(0) {
    reset();
  }

  exponential_filter(float sampling_period, float time_constant) {
    init(sampling_period, time_constant);
    reset();
  }

  void push(T input_value) {
    out_ = out_ + smooth_factor_ * (input_value - out_);
  }

  T output() const { return out_; }

  void set_output(T value) { out_ = value; }

  void reset() { set_output(T(0)); }

  void init(float sampling_period, float time_constant) {
    sampling_period_ = sampling_period;
    time_constant_ = time_constant;
    smooth_factor_ = emb::clamp(sampling_period / time_constant, 0.f, 1.f);
  }

  void set_sampling_period(float value) {
    sampling_period_ = value;
    smooth_factor_ = emb::clamp(sampling_period_ / time_constant_, 0.f, 1.f);
  }

  float smooth_factor() const { return smooth_factor_; }
};

} // namespace emb
