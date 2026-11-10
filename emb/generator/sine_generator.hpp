#pragma once

#include <algorithm>
#include <cassert>
#include <emb/algorithm.hpp>
#include <emb/math.hpp>
#include <emb/units.hpp>

namespace emb {

template<typename T>
class sine_generator {
public:
  using value_type = T;
private:
  units::sec_f32 timestep_;
  value_type ampl_;
  float wfreq_;
  emb::units::rad_f32 init_phase_;
  value_type bias_;

  emb::units::rad_f32 phase_;
  value_type output_;
public:
  constexpr sine_generator(
      units::sec_f32 timestep,
      value_type ampl,
      units::hz_f32 freq,
      emb::units::rad_f32 init_phase = emb::units::rad_f32(0),
      value_type bias = value_type{})
      : timestep_(timestep),
        ampl_(ampl),
        wfreq_(2 * std::numbers::pi_v<float> * freq.value),
        init_phase_(init_phase),
        bias_(bias)
  {
    assert(timestep.value > 0);
    reset();
  }

  constexpr value_type output() const
  {
    return output_;
  }

  constexpr void reset()
  {
    phase_ = init_phase_;
    output_ = ampl_ * emb::sin(phase_.value) + bias_;
  }

  constexpr void step()
  {
    phase_ = emb::units::rad_f32(
        emb::norm2pi(phase_.value + wfreq_ * timestep_.value));
    output_ = ampl_ * emb::sin(phase_.value) + bias_;
  }

  constexpr units::sec_f32 timestep() const
  {
    return timestep_;
  }

  constexpr value_type ampl() const
  {
    return ampl_;
  }

  constexpr value_type bias() const
  {
    return bias_;
  }

  constexpr float freq() const
  {
    return wfreq_ / (2 * std::numbers::pi_v<float>);
  }

  constexpr emb::units::rad_f32 phase() const
  {
    return phase_;
  }
};

} // namespace emb
