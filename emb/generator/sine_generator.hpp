#pragma once

#include <algorithm>
#include <emb/algorithm.hpp>
#include <emb/core.hpp>
#include <emb/math.hpp>
#include <emb/units.hpp>

namespace emb {

template<typename T>
class sine_generator {
public:
  typedef T output_type;
  typedef output_type const& const_reference;
private:
  units::sec_f32 update_period_;
  output_type ampl_;
  float wfreq_;
  emb::units::rad_f32 init_phase_;
  output_type bias_;

  emb::units::rad_f32 phase_;
  output_type output_;
public:
  constexpr sine_generator(
      units::sec_f32 const& update_period,
      output_type const& ampl,
      units::hz_f32 const& freq,
      emb::units::rad_f32 const& init_phase = emb::units::rad_f32(0),
      output_type bias = output_type()
  )
      : update_period_(update_period),
        ampl_(ampl),
        wfreq_(2 * std::numbers::pi_v<float> * freq.value()),
        init_phase_(init_phase),
        bias_(bias) {
    assert(update_period.value() > 0);
    reset();
  }

  constexpr const_reference output() const {
    return output_;
  }

  constexpr void reset() {
    phase_ = init_phase_;
    output_ = ampl_ * emb::sin(phase_.value()) + bias_;
  }

  constexpr void update() {
    phase_ = emb::units::rad_f32(
        emb::norm2pi(phase_.value() + wfreq_ * update_period_.value())
    );
    output_ = ampl_ * emb::sin(phase_.value()) + bias_;
  }

  constexpr units::sec_f32 update_period() const {
    return update_period_;
  }

  constexpr const_reference ampl() const {
    return ampl_;
  }

  constexpr const_reference bias() const {
    return bias_;
  }

  constexpr float freq() const {
    return wfreq_ / (2 * std::numbers::pi_v<float>);
  }

  constexpr emb::units::rad_f32 phase() const {
    return phase_;
  }
};

} // namespace emb
