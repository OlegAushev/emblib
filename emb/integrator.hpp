#pragma once

#include <emb/units.hpp>

#include <algorithm>
#include <concepts>
#include <limits>

namespace emb {

template<std::floating_point T>
class integrator {
public:
  using value_type = T;
private:
  units::sec<value_type> ts_;
  value_type lower_limit_;
  value_type upper_limit_;
  value_type output_;
public:
  constexpr integrator(
      units::sec<value_type> timestep,
      value_type init_output = value_type{},
      value_type lower_limit = -std::numeric_limits<value_type>::max(),
      value_type upper_limit = std::numeric_limits<value_type>::max())
      : ts_(timestep), lower_limit_(lower_limit), upper_limit_(upper_limit)
  {
    reset(init_output);
  }

  constexpr void step(value_type rate)
  {
    output_ = clamp(output_ + rate * ts_.value);
  }

  constexpr value_type output() const
  {
    return output_;
  }

  constexpr void reset(value_type value)
  {
    output_ = clamp(value);
  }

  constexpr void set_timestep(units::sec<value_type> value)
  {
    ts_ = value;
  }

  constexpr void set_limits(value_type lower, value_type upper)
  {
    lower_limit_ = lower;
    upper_limit_ = upper;
  }

  constexpr value_type lower_limit() const
  {
    return lower_limit_;
  }

  constexpr value_type upper_limit() const
  {
    return upper_limit_;
  }
private:
  constexpr value_type clamp(value_type value) const
  {
    return std::clamp(value, lower_limit_, upper_limit_);
  }
};

} // namespace emb
