#pragma once

#include <emb/foc/types.hpp>
#include <emb/math.hpp>

#include <algorithm>
#include <array>

namespace emb {
namespace foc {

inline std::array<unsigned_pu_f32, 3> compensate_deadtime_v1(
    std::array<unsigned_pu_f32, 3> const& dutycycles,
    current_abc const& currents,
    float current_threshold,
    float pwm_period,
    float deadtime
) {
  std::array<unsigned_pu_f32, 3> dc;
  emb::unsigned_pu_f32 const deadtime_dutycycle(deadtime / pwm_period);
  std::array<float, 3> const i_ph{currents.a, currents.b, currents.c};

  for (auto i = 0uz; i < 3; ++i) {
    if (i_ph[i] > current_threshold) {
      dc[i] = dutycycles[i] + deadtime_dutycycle;
    } else if (i_ph[i] < -current_threshold) {
      dc[i] = dutycycles[i] - deadtime_dutycycle;
    } else {
      dc[i] = dutycycles[i];
    }
  }

  return dc;
}

/// @brief DOI: 10.4028/www.scientific.net/AMM.416-417.536
inline std::array<unsigned_pu_f32, 3> compensate_deadtime_v2(
    std::array<unsigned_pu_f32, 3> const& dutycycles,
    current_abc const& currents,
    float current_threshold,
    float pwm_period,
    float deadtime
) {
#ifdef __c28x__
  return dutycycles;
#else
  auto dc = dutycycles;
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

  return dc;
#endif
}

} // namespace foc
} // namespace emb
