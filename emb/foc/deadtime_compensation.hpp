#pragma once

#include <emb/foc/types.hpp>
#include <emb/math.hpp>

#include <algorithm>
#include <array>

namespace emb {
namespace foc {

inline three_phase<unsigned_pu_f32> compensate_deadtime_v1(
    three_phase<unsigned_pu_f32> const& dutycycles,
    current_abc const& currents,
    float current_threshold,
    float pwm_period,
    float deadtime
) {
  emb::unsigned_pu_f32 const deadtime_dutycycle(deadtime / pwm_period);

  auto const compensate = [&](unsigned_pu_f32 duty, float current) {
    if (current > current_threshold) {
      return duty + deadtime_dutycycle;
    } else if (current < -current_threshold) {
      return duty - deadtime_dutycycle;
    } else {
      return duty;
    }
  };

  return {
      .a = compensate(dutycycles.a, currents.a),
      .b = compensate(dutycycles.b, currents.b),
      .c = compensate(dutycycles.c, currents.c)
  };
}

/// @brief DOI: 10.4028/www.scientific.net/AMM.416-417.536
inline three_phase<unsigned_pu_f32> compensate_deadtime_v2(
    three_phase<unsigned_pu_f32> const& dutycycles,
    current_abc const& currents,
    float current_threshold,
    float pwm_period,
    float deadtime
) {
#ifdef __c28x__
  return dutycycles;
#else
  std::array<unsigned_pu_f32, 3> dc{dutycycles.a, dutycycles.b, dutycycles.c};
  emb::unsigned_pu_f32 const deadtime_dutycycle(deadtime / pwm_period);

  std::array<float, 3> const i_ph{currents.a, currents.b, currents.c};

  auto const [min, max] = std::minmax_element(i_ph.begin(), i_ph.end());

  // use Kirchhoff's current law to determine
  // if there is one positive or one negative current
  if (*min + *max > 0) {
    auto const idx = std::distance(i_ph.begin(), max);
    dc[std::size_t(idx)] = dc[std::size_t(idx)] + 2 * deadtime_dutycycle;
  } else if (*min + *max < 0) {
    auto const idx = std::distance(i_ph.begin(), min);
    dc[std::size_t(idx)] = dc[std::size_t(idx)] - 2 * deadtime_dutycycle;
  }

  return {.a = dc[0], .b = dc[1], .c = dc[2]};
#endif
}

} // namespace foc
} // namespace emb
