#pragma once

#include <emb/foc/pipe.hpp>
#include <emb/motorcontrol.hpp>

#include <array>

namespace emb {
namespace foc {

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
