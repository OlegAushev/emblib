#pragma once

#include <emb/algorithm.hpp>
#include <emb/units.hpp>

#include <algorithm>
#include <cassert>

namespace emb {

template<typename T>
class ramp_generator {
public:
  using value_type = T;
private:
  units::sec_f32 ts_;
  value_type slope_;
  value_type step_;

  value_type target_;
  value_type output_;
public:
  constexpr ramp_generator(units::sec_f32 timestep,
                           value_type slope,
                           value_type init_output = value_type{})
  {
    set_slope(timestep, slope);
    reset(init_output);
  }

  constexpr value_type target() const
  {
    return target_;
  }

  constexpr value_type output() const
  {
    return output_;
  }

  constexpr void set_target(value_type value)
  {
    target_ = value;
  }

  constexpr void reset(value_type value)
  {
    target_ = value;
    output_ = value;
  }

  constexpr void set_slope(units::sec_f32 timestep, value_type slope)
  {
    assert(timestep.value > 0);
    assert(slope > T(0));
    ts_ = timestep;
    slope_ = slope;
    step_ = timestep.value * slope;
  }

  constexpr void set_timestep(units::sec_f32 ts)
  {
    set_slope(ts, slope_);
  }

  constexpr void step()
  {
    if (output_ < target_) {
      output_ = std::min(output_ + step_, target_);
    }
    else {
      output_ = std::max(output_ - step_, target_);
    }
  }

  constexpr bool at_target() const
  {
    return output_ == target_;
  }
};

} // namespace emb
