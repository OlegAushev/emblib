#pragma once

#include <emb/algorithm.hpp>
#include <emb/assert.hpp>
#include <emb/units.hpp>

#include <algorithm>

namespace emb {

template<typename T>
class ramp_generator {
public:
  using value_type = T;
  using const_reference = value_type const&;
private:
  units::sec_f32 ts_;
  value_type slope_;
  value_type step_;

  value_type target_;
  value_type init_output_;
  value_type output_;
public:
  constexpr ramp_generator(
      units::sec_f32 const& timestep,
      value_type const& slope,
      value_type const& init_output = T{}
  )
      : init_output_(init_output) {
    set_slope(timestep, slope);
    reset();
  }

  constexpr value_type target() const {
    return target_;
  }

  constexpr value_type output() const {
    return output_;
  }

  constexpr void set_target(const_reference value) {
    target_ = value;
  }

  constexpr void set_output(const_reference value) {
    target_ = value;
    output_ = value;
  }

  constexpr void reset() {
    set_output(init_output_);
  }

  constexpr void
  set_slope(units::sec_f32 const& timestep, const_reference slope) {
    assert(timestep.value() > 0);
    assert(slope > T(0));
    ts_ = timestep;
    slope_ = slope;
    step_ = timestep.value() * slope;
  }

  constexpr void set_timestep(units::sec_f32 const& ts) {
    set_slope(ts, slope_);
  }

  constexpr void update() {
    if (output_ < target_) {
      output_ = std::min(output_ + step_, target_);
    } else {
      output_ = std::max(output_ - step_, target_);
    }
  }

  constexpr bool at_target() const {
    return output_ == target_;
  }
};

} // namespace emb
