#pragma once

#include <emb/algorithm.hpp>
#include <emb/units.hpp>

#include <algorithm>

namespace emb {

template<typename T>
class ramp_generator {
public:
  using value_type = T;
  using const_reference = value_type const& ;
private:
  units::sec_f32 ts_;
  value_type slope_;
  value_type step_;

  value_type target_;
  value_type init_output_;
  value_type output_;
public:
  ramp_generator(
      units::sec_f32 const& ts,
      value_type const& slope,
      value_type const& init_output = T{}
  )
      : init_output_(init_output) {
    set_slope(ts, slope);
    reset();
  }

  value_type target() const {
    return target_;
  }

  value_type output() const {
    return output_;
  }

  void set_target(const_reference input_v) {
    target_ = input_v;
  }

  void set_output(const_reference output_v) {
    target_ = output_v;
    output_ = output_v;
  }

  void reset() {
    set_output(init_output_);
  }

  void set_slope(units::sec_f32 const& ts, const_reference slope) {
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
