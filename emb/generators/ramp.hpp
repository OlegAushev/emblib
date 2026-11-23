#pragma once

#include <emb/meta/scalar_wrapper.hpp>
#include <emb/units/chrono.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstdint>

namespace emb::generators {

namespace detail {

class ramp_f32 {
public:
  constexpr explicit ramp_f32(float step, float initial = 0.0f)
      : step_(step), base_(initial), target_(initial), output_(initial)
  {
    assert(step > 0.0f && std::isfinite(step));
  }

  constexpr float output() const
  {
    return output_;
  }

  constexpr float target() const
  {
    return target_;
  }

  constexpr bool at_target() const
  {
    return output_ == target_;
  }

  constexpr void reset(float value)
  {
    output_ = target_ = value;
    rebase();
  }

  constexpr void set_step(float step)
  {
    assert(step > 0.0f && std::isfinite(step));
    if (step != step_) {
      step_ = step;
      rebase();
    }
  }

  constexpr void set_target(float value)
  {
    if (value == target_) return;

    bool const was_rising = target_ > output_;
    bool const will_rise = value > output_;
    if (at_target() || was_rising != will_rise) rebase();
    target_ = value;
  }

  constexpr void update()
  {
    if (at_target()) return;

    bool const rising = target_ > output_;
    ++tick_;
    float const next =
        std::fma(rising ? step_ : -step_, static_cast<float>(tick_), base_);
    output_ = rising ? std::min(next, target_) : std::max(next, target_);

    if (tick_ == max_exact_tick) rebase();
  }

  constexpr void update_toward(float value)
  {
    set_target(value);
    update();
  }

private:
  constexpr void rebase()
  {
    base_ = output_;
    tick_ = 0;
  }

  static constexpr std::uint32_t max_exact_tick = 1u << 24;
  float step_;
  float base_;
  float target_;
  float output_;
  std::uint32_t tick_{0};
};

} // namespace detail

// Generates a linear ramp in float and returns its output and target as T.
// T may be float or a scalar wrapper of float. For a wrapper, every finite
// float x between the start and target of a segment, including both endpoints,
// must satisfy scalar_of(T{x}) == x. If T defines equality, it must agree
// with equality of the wrapped values on this interval.
//
// The ramp constructs T from the product of the timestep and the scalar slope,
// then uses its scalar value as the step. A bounded wrapper can therefore
// saturate the step. For signed_pu_f32, a product of 2 gives a step of 1.
//
// at_target() compares the stored float output and target. A wrapper that
// quantizes intermediate values can present the target before this comparison
// becomes true.
template<typename T>
  requires std::same_as<emb::unwrapped_t<T>, float>
class ramp {
public:
  using value_type = T;

  constexpr ramp(units::sec_f32 timestep,
                 value_type slope,
                 value_type initial = value_type{})
      : slope_(slope), core_(make_step(timestep, slope), scalar_of(initial))
  {
  }

  constexpr value_type output() const
  {
    return value_type{core_.output()};
  }

  constexpr value_type target() const
  {
    return value_type{core_.target()};
  }

  constexpr bool at_target() const
  {
    return core_.at_target();
  }

  constexpr void reset(value_type value)
  {
    core_.reset(scalar_of(value));
  }

  constexpr void set_target(value_type value)
  {
    core_.set_target(scalar_of(value));
  }

  constexpr void update()
  {
    core_.update();
  }

  constexpr void update_toward(value_type value)
  {
    core_.update_toward(scalar_of(value));
  }

  constexpr void set_slope(units::sec_f32 timestep, value_type slope)
  {
    core_.set_step(make_step(timestep, slope));
    slope_ = slope;
  }

  constexpr void set_timestep(units::sec_f32 timestep)
  {
    set_slope(timestep, slope_);
  }

private:
  static constexpr float make_step(units::sec_f32 timestep, value_type slope)
  {
    float const s = scalar_of(slope);
    assert(timestep.value > 0.0f && s > 0.0f);
    return scalar_of(value_type{timestep.value * s});
  }

  value_type slope_;
  detail::ramp_f32 core_;
};

} // namespace emb::generators
