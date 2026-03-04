#pragma once

#include <emb/foc/types.hpp>
#include <emb/math.hpp>

#include <algorithm>
#include <array>
#include <numbers>

namespace emb {
namespace foc {

inline std::array<emb::unsigned_pu, 3>
calculate_svpwm(vec_polar v_s, float v_dc) {
  v_s.theta = rem2pi(v_s.theta);
  v_s.mag = std::clamp<float>(v_s.mag, 0, v_dc / std::numbers::sqrt3_v<float>);

  int32_t const sector = static_cast<int32_t>(
      v_s.theta / (std::numbers::pi_v<float> / 3.0f)
  );
  float const theta = v_s.theta -
                      float(sector) * (std::numbers::pi_v<float> / 3.0f);

  // base vector times calculation
  float const tb1 = std::numbers::sqrt3_v<float> *
                    (v_s.mag / v_dc) *
                    emb::sin((std::numbers::pi_v<float> / 3.0f) - theta);
  float const tb2 = std::numbers::sqrt3_v<float> *
                    (v_s.mag / v_dc) *
                    emb::sin(theta);
  float const tb0 = (1.f - tb1 - tb2) / 2.f;

  std::array<float, 3> pulse_durations;
  switch (sector) {
  case 0:
    pulse_durations[0] = tb1 + tb2 + tb0;
    pulse_durations[1] = tb2 + tb0;
    pulse_durations[2] = tb0;
    break;
  case 1:
    pulse_durations[0] = tb1 + tb0;
    pulse_durations[1] = tb1 + tb2 + tb0;
    pulse_durations[2] = tb0;
    break;
  case 2:
    pulse_durations[0] = tb0;
    pulse_durations[1] = tb1 + tb2 + tb0;
    pulse_durations[2] = tb2 + tb0;
    break;
  case 3:
    pulse_durations[0] = tb0;
    pulse_durations[1] = tb1 + tb0;
    pulse_durations[2] = tb1 + tb2 + tb0;
    break;
  case 4:
    pulse_durations[0] = tb2 + tb0;
    pulse_durations[1] = tb0;
    pulse_durations[2] = tb1 + tb2 + tb0;
    break;
  case 5:
    pulse_durations[0] = tb1 + tb2 + tb0;
    pulse_durations[1] = tb0;
    pulse_durations[2] = tb1 + tb0;
    break;
  default: break;
  }

  std::array<emb::unsigned_pu, 3> duty_cycles;
  for (size_t i = 0; i < 3; ++i) {
    duty_cycles[i] = emb::unsigned_pu(pulse_durations[i]);
  }

  return duty_cycles;
}

class svpwm {
  float vDC_;
public:
  constexpr svpwm(float vDC) : vDC_(vDC) {}

  std::array<unsigned_pu, 3> operator()(vec_polar v) const {
    return calculate_svpwm(v, vDC_);
  }
};

} // namespace foc
} // namespace emb
