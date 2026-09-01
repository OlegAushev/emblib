#pragma once

#include <emb/foc/types.hpp>
#include <emb/math.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <numbers>

namespace emb {
namespace foc {

inline three_phase<emb::unsigned_pu_f32>
calculate_svpwm(voltage_polar v_s, float v_dc) {
  v_s.theta = norm2pi(v_s.theta);
  v_s.mag = std::clamp<float>(v_s.mag, 0, v_dc / std::numbers::sqrt3_v<float>);

  std::int32_t const sector = static_cast<std::int32_t>(
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

  return {
      .a = emb::unsigned_pu_f32(pulse_durations[0]),
      .b = emb::unsigned_pu_f32(pulse_durations[1]),
      .c = emb::unsigned_pu_f32(pulse_durations[2])
  };
}

inline three_phase<emb::unsigned_pu_f32>
calculate_svpwm_v2(voltage_abc const& Vs, float Vdc) {
  // normalization: [−1, +1]
  float const inv = 2.f / Vdc;
  float const Va = Vs.a * inv;
  float const Vb = Vs.b * inv;
  float const Vc = Vs.c * inv;

  auto const [Vmin, Vmax] = std::minmax({Va, Vb, Vc});

  // common-mode offset
  float const Voff = -0.5f * (Vmax + Vmin);

  // duty cycles
  return {
      .a = emb::unsigned_pu_f32((Va + Voff + 1.f) * 0.5f),
      .b = emb::unsigned_pu_f32((Vb + Voff + 1.f) * 0.5f),
      .c = emb::unsigned_pu_f32((Vc + Voff + 1.f) * 0.5f)
  };
}

class svpwm {
private:
  float Vdc_;
public:
  constexpr svpwm(float Vdc) : Vdc_(Vdc) {}

  three_phase<unsigned_pu_f32> operator()(voltage_polar V) const {
    return calculate_svpwm(V, Vdc_);
  }
};

} // namespace foc
} // namespace emb
