#pragma once

#include <emb/foc/types.hpp>
#include <emb/math.hpp>

#include <algorithm>
#include <array>

namespace emb {
namespace foc {

inline std::array<unsigned_pu, 3> compensate_deadtime_v1(
    std::array<unsigned_pu, 3> const& dutycycles,
    std::array<float, 3> const& currents,
    float current_threshold,
    float pwm_period,
    float deadtime
) {
  std::array<unsigned_pu, 3> dc;
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
inline std::array<unsigned_pu, 3> compensate_deadtime_v2(
    std::array<unsigned_pu, 3> const& dutycycles,
    std::array<float, 3> const& currents,
    float current_threshold,
    float pwm_period,
    float deadtime
) {
#ifdef __c28x__
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

} // namespace foc
} // namespace emb
