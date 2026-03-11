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

  std::array<float, 3> pulse_durations{};
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

inline std::array<emb::unsigned_pu, 3>
calculate_svpwm_v2(std::array<float, 3> const& Vs, float Vdc) {
  // normalization: [−1, +1]
  float const inv = 2.f / Vdc;
  float const Va = Vs[0] * inv;
  float const Vb = Vs[1] * inv;
  float const Vc = Vs[2] * inv;

  auto const [Vmin, Vmax] = std::minmax({Va, Vb, Vc});

  // common-mode offset
  float const Voff = -0.5f * (Vmax + Vmin);

  // duty cycles
  std::array<emb::unsigned_pu, 3> duty;
  duty[0] = emb::unsigned_pu((Va + Voff + 1.f) * 0.5f);
  duty[1] = emb::unsigned_pu((Vb + Voff + 1.f) * 0.5f);
  duty[2] = emb::unsigned_pu((Vc + Voff + 1.f) * 0.5f);
  return duty;
}

class svpwm {
private:
  float Vdc_;
public:
  constexpr svpwm(float Vdc) : Vdc_(Vdc) {}

  std::array<unsigned_pu, 3> operator()(vec_polar V) const {
    return calculate_svpwm(V, Vdc_);
  }
};

} // namespace foc
} // namespace emb
