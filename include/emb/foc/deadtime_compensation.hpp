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

class deadtime_compensation {
  std::array<float, 3> currents_;
  float current_threshold_;
  float pwm_period_;
  float deadtime_;
public:
  constexpr deadtime_compensation(
      std::array<float, 3> const& currents,
      float current_threshold,
      float pwm_period,
      float dt
  )
      : currents_(currents),
        current_threshold_(current_threshold),
        pwm_period_(pwm_period),
        deadtime_(dt) {}

  std::array<unsigned_pu, 3>
  operator()(std::array<unsigned_pu, 3> const& dutycycles) const {
    return compensate_deadtime_v2(
        dutycycles, currents_, current_threshold_, pwm_period_, deadtime_
    );
  }
};

} // namespace foc
} // namespace emb
