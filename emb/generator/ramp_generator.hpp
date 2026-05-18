#pragma once

#include <emb/algorithm.hpp>
#include <emb/core.hpp>
#include <emb/units.hpp>

#include <algorithm>

namespace emb {

template<typename T>
class ramp_generator {
private:
  units::sec_f32 ts_;
  T slope_;
  T step_;

  T target_;
  T init_output_;
  T output_;
public:
  ramp_generator(
      units::sec_f32 const& ts,
      T const& slope,
      T const& init_output = T{}
  )
      : init_output_(init_output) {
    set_slope(ts, slope);
    reset();
  }

  T target() const {
    return target_;
  }

  T output() const {
    return output_;
  }

  void set_target(T const& input_v) {
    target_ = input_v;
  }

  void set_output(T const& output_v) {
    target_ = output_v;
    output_ = output_v;
  }

  void reset() {
    set_output(init_output_);
  }

  void set_slope(units::sec_f32 const& ts, T const& slope) {
    assert(ts.value() > 0);
    assert(slope > T(0));
    ts_ = ts;
    slope_ = slope;
    step_ = ts.value() * slope;
  }

  void set_timestep(units::sec_f32 const& ts) {
    set_slope(ts, slope_);
  }

  void update() {
    if (output_ < target_) {
      output_ = std::min(output_ + step_, target_);
    } else {
      output_ = std::max(output_ - step_, target_);
    }
  }

  bool at_target() const {
    return output_ == target_;
  }
};

} // namespace emb
