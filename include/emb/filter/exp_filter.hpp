#pragma once

#include <algorithm>
#include <utility>

namespace emb {
namespace filter {

template<typename T, typename Duration>
class exponential {
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
  exponential(
      duration_type sampling_period,
      duration_type time_constant,
      value_type const& init_output = value_type()
  )
      : init_output_(init_output) {
    configure(sampling_period, time_constant);
    reset();
  }

  void push(value_type const& input_v) {
    output_ = output_ + smooth_factor_ * (input_v - output_);
  }

  value_type output() const {
    return output_;
  }

  void set_output(value_type const& output_v) {
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
