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

  T ref_;
  T out_;
public:
  ramp_generator() : update_period_(0), slope_(T()), step_(T()) { reset(); }

  ramp_generator(float update_period, T slope) {
    init(update_period, slope);
    reset();
  }

  void push(T input_value) { ref_ = input_value; }

  T output() const { return out_; }

  void set_output(T value) {
    ref_ = value;
    out_ = value;
  }

  void reset() { set_output(T()); }

  void init(float update_period, T slope) {
    assert(update_period > 0);
    assert(slope > T(0));
    update_period_ = update_period;
    slope_ = slope;
    step_ = update_period * slope;
  }

  void update() {
    if (out_ < ref_) {
      out_ = std::min(out_ + step_, ref_);
    } else {
      out_ = std::max(out_ - step_, ref_);
    }
  }

  bool steady() const { return out_ == ref_; }
};

} // namespace emb
