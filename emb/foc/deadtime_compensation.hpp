#pragma once

#include <emb/foc/types.hpp>
#include <emb/math.hpp>

namespace emb {
namespace foc {

inline three_phase<unsigned_pu_f32>
compensate_deadtime(three_phase<unsigned_pu_f32> const& dutycycles,
                    current_abc const& currents,
                    float current_threshold,
                    float pwm_period,
                    float deadtime)
{
  emb::unsigned_pu_f32 const deadtime_dutycycle(deadtime / pwm_period);

  auto const compensate = [&](unsigned_pu_f32 duty, float current) {
    if (current > current_threshold) {
      return duty + deadtime_dutycycle;
    }
    else if (current < -current_threshold) {
      return duty - deadtime_dutycycle;
    }
    else {
      return duty;
    }
  };

  return {.a = compensate(dutycycles.a, currents.a),
          .b = compensate(dutycycles.b, currents.b),
          .c = compensate(dutycycles.c, currents.c)};
}

} // namespace foc
} // namespace emb
