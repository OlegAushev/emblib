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
  value_type init_output_;
  value_type output_;
public:
  constexpr integrator(
      units::sec<value_type> timestep,
      value_type init_output = 0,
      value_type lower_limit = -std::numeric_limits<value_type>::max(),
      value_type upper_limit = std::numeric_limits<value_type>::max()
  )
      : ts_(timestep),
        lower_limit_(lower_limit),
        upper_limit_(upper_limit),
        init_output_(init_output) {
    reset();
  }

  constexpr void push(value_type rate) {
    set_output(output_ + rate * ts_.value());
  }

  constexpr void add(value_type increment) {
    set_output(output_ + increment);
  }

  constexpr value_type output() const {
    return output_;
  }

  constexpr void set_output(value_type value) {
    output_ = std::clamp(value, lower_limit_, upper_limit_);
  }

  constexpr void reset() {
    set_output(init_output_);
  }

  constexpr void set_timestep(units::sec<value_type> value) {
    ts_ = value;
  }

  constexpr void set_lower_limit(value_type value) {
    lower_limit_ = value;
  }

  constexpr void set_upper_limit(value_type value) {
    upper_limit_ = value;
  }

  constexpr value_type lower_limit() const {
    return lower_limit_;
  }

  constexpr value_type upper_limit() const {
    return upper_limit_;
  }
};

} // namespace emb
