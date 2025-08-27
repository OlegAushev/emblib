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
  float update_period_;
  output_type ampl_;
  float wfreq_;
  emb::units::angle_t init_phase_;
  output_type bias_;

  float phase_;
  output_type output_;
public:
  EMB_CONSTEXPR sine_generator(
      float update_period,
      output_type const& ampl,
      float const& freq,
      emb::units::angle_t const& init_phase = emb::units::angle_t(),
      output_type bias = output_type())
      : update_period_(update_period),
        ampl_(ampl),
        wfreq_(2 * emb::numbers::pi * freq),
        init_phase_(init_phase),
        bias_(bias) {
    assert(update_period > 0);
    reset();
  }

  EMB_CONSTEXPR const_reference output() const { return output_; }

  EMB_CONSTEXPR void reset() {
    phase_ = init_phase_.rad().numval();
    output_ = ampl_ * emb::sin(phase_) + bias_;
  }

  EMB_CONSTEXPR void update() {
    phase_ = emb::rem2pi(phase_ + wfreq_ * update_period_);
    output_ = ampl_ * emb::sin(phase_) + bias_;
  }

  EMB_CONSTEXPR float update_period() const { return update_period_; }

  EMB_CONSTEXPR const_reference ampl() const { return ampl_; }

  EMB_CONSTEXPR const_reference bias() const { return bias_; }

  EMB_CONSTEXPR float freq() const { return wfreq_ / (2 * emb::numbers::pi); }

  emb::units::angle_t phase() const {
    return emb::units::angle_t(emb::units::rad_f32(phase_));
  }
};

#ifdef __cpp_concepts

template<typename T>
struct is_sine_generator : std::false_type {};

template<typename T>
struct is_sine_generator<emb::sine_generator<T>> : std::true_type {};

template<typename T>
concept SineGenerator = is_sine_generator<T>::value;

#endif

} // namespace emb
