#pragma once

#include <emblib/array.hpp>
#include <emblib/core.hpp>
#include <emblib/math.hpp>
#include <emblib/scopedenum.hpp>
#include <emblib/units.hpp>

namespace emb {

#if __cplusplus >= 201100
enum class phase3 : uint32_t {
  a,
  b,
  c
};
#else
// clang-format off
SCOPED_ENUM_UT_DECLARE_BEGIN(phase3, uint32_t) {
  a,
  b,
  c
} SCOPED_ENUM_DECLARE_END(phase3);
// clang-format on
#endif

struct vec_alpha {
  float mag;
  float theta;
};

struct vec_alphabeta {
  float alpha;
  float beta;
};

struct vec_dq {
  float d;
  float q;
};

inline vec_dq park_transform(vec_alphabeta v, float sine, float cosine) {
  vec_dq retv;
  retv.d = (v.alpha * cosine) + (v.beta * sine);
  retv.q = (v.beta * cosine) - (v.alpha * sine);
  return retv;
}

inline vec_alphabeta invpark_transform(vec_dq v, float sine, float cosine) {
  vec_alphabeta retv;
  retv.alpha = (v.d * cosine) - (v.q * sine);
  retv.beta = (v.q * cosine) + (v.d * sine);
  return retv;
}

inline vec_alphabeta clarke_transform(float a, float b, float c) {
  vec_alphabeta retv;
  retv.alpha = a;
  retv.beta = (b - c) * numbers::inv_sqrt3;
  return retv;
}

inline vec_alphabeta clarke_transform(emb::array<float, 3> const& v) {
  vec_alphabeta retv;
  retv.alpha = v[0];
  retv.beta = (v[1] - v[2]) * numbers::inv_sqrt3;
  return retv;
}

inline vec_alphabeta clarke_transform(float a, float b) {
  vec_alphabeta retv;
  retv.alpha = a;
  retv.beta = (a + 2 * b) * numbers::inv_sqrt3;
  return retv;
}

inline emb::array<float, 3> invclarke_transform(vec_alphabeta v) {
  emb::array<float, 3> retv;
  retv[0] = v.alpha;
  retv[1] = (-v.alpha + emb::numbers::sqrt_3 * v.beta) * 0.5f;
  retv[2] = (-v.alpha - emb::numbers::sqrt_3 * v.beta) * 0.5f;
  return retv;
}

inline emb::array<emb::unsigned_pu, 3> calculate_sinpwm(vec_alphabeta v_s,
                                                        float v_dc) {
  emb::array<float, 3> voltages = invclarke_transform(v_s);
  float const voltage_base = v_dc / 1.5f;
  emb::array<emb::unsigned_pu, 3> duty_cycles;

  for (size_t i = 0; i < 3; ++i) {
    duty_cycles[i] = emb::unsigned_pu(voltages[i] / voltage_base);
  }

  return duty_cycles;
}

inline emb::array<emb::unsigned_pu, 3> calculate_svpwm(vec_alpha v_s,
                                                       float v_dc) {
  v_s.theta = rem_2pi(v_s.theta);
  v_s.mag = clamp<float>(v_s.mag, 0, v_dc / numbers::sqrt_3);

  int32_t sector = static_cast<int32_t>(v_s.theta / numbers::pi_over_3);
  float theta = v_s.theta - float(sector) * numbers::pi_over_3;

  // base vector times calculation
#if defined(EMBLIB_C28X)
  float tb1 =
      numbers::sqrt_3 * (v_s.mag / v_dc) * sinf(numbers::pi_over_3 - theta);
  float tb2 = numbers::sqrt_3 * (v_s.mag / v_dc) * sinf(theta);
#elif defined(EMBLIB_ARM)
  float tb1 = numbers::sqrt_3 * (v_s.mag / v_dc) *
              arm_sin_f32(numbers::pi_over_3 - theta);
  float tb2 = numbers::sqrt_3 * (v_s.mag / v_dc) * arm_sin_f32(theta);
#else
#error "emblib error: arch not defined!"
#endif
  float tb0 = (1.f - tb1 - tb2) / 2.f;

  emb::array<float, 3> pulse_durations;
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
  default:
    break;
  }

  emb::array<emb::unsigned_pu, 3> duty_cycles;
  for (size_t i = 0; i < 3; ++i) {
    duty_cycles[i] = emb::unsigned_pu(pulse_durations[i]);
  }

  return duty_cycles;
}

inline emb::array<unsigned_pu, 3>
compensate_deadtime_v1(emb::array<unsigned_pu, 3> const& dutycycles,
                       emb::array<float, 3> const& currents,
                       float current_threshold,
                       float pwm_period,
                       float deadtime) {
  emb::array<unsigned_pu, 3> dc;
  emb::unsigned_pu const deadtime_dutycycle(deadtime / pwm_period);

  for (size_t i = 0; i < 3; ++i) {
    if (currents[i] > current_threshold) {
      dc[i] = dutycycles[i] + deadtime_dutycycle;
    } else if (currents[i] < -current_threshold) {
      dc[i] = dutycycles[i] - deadtime_dutycycle;
    } else {
      dc[i] = dutycycles[i];
    }
  }

  return dc;
}

/// @brief DOI: 10.4028/www.scientific.net/AMM.416-417.536
inline emb::array<unsigned_pu, 3>
compensate_deadtime_v2(emb::array<unsigned_pu, 3> const& dutycycles,
                       emb::array<float, 3> const& currents,
                       float current_threshold,
                       float pwm_period,
                       float deadtime) {
#ifdef EMBLIB_C28X
  return dutycycles;
#else
  auto dc = dutycycles;
  emb::unsigned_pu const deadtime_dutycycle(deadtime / pwm_period);

  auto const [min, max] = std::minmax_element(currents.begin(), currents.end());

  // use Kirchhoff's current law to determine if there is one positive or one negative current
  if (*min + *max > 0) {
    auto const idx = std::distance(currents.begin(), max);
    dc[size_t(idx)] = dc[size_t(idx)] + 2 * deadtime_dutycycle;
  } else if (*min + *max < 0) {
    auto const idx = std::distance(currents.begin(), min);
    dc[size_t(idx)] = dc[size_t(idx)] - 2 * deadtime_dutycycle;
  }

  return dc;
#endif
}

} // namespace emb
