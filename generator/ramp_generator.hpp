#pragma once

#include <algorithm>
#include <emblib/algorithm.hpp>
#include <emblib/core.hpp>

namespace emb {

template<typename T>
class ramp_generator {
private:
  float update_period_;
  T slope_;
  T step_;

  T target_;
  T init_output_;
  T output_;
public:
  ramp_generator(
      float update_period,
      T const& slope,
      T const& init_output = T())
      : init_output_(init_output) {
    configure(update_period, slope);
    reset();
  }

  T target() const { return target_; }

  T output() const { return output_; }

  void set_target(T const& input_v) { target_ = input_v; }

  void set_output(T const& output_v) {
    target_ = output_v;
    output_ = output_v;
  }

  void reset() { set_output(init_output_); }

  void configure(float update_period, T slope) {
    assert(update_period > 0);
    assert(slope > T(0));
    update_period_ = update_period;
    slope_ = slope;
    step_ = update_period * slope;
  }

  void update() {
    if (output_ < target_) {
      output_ = std::min(output_ + step_, target_);
    } else {
      output_ = std::max(output_ - step_, target_);
    }
  }

  bool steady() const { return output_ == target_; }
};

} // namespace emb
