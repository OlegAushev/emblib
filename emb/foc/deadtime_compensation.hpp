#pragma once

#include <emb/foc/types.hpp>
#include <emb/math.hpp>

#include <algorithm>

namespace emb {
namespace foc {

// Returns `dutycycles` corrected for the deadtime of the inverter legs. While
// the current of a leg flows out into the load, positive in `currents`, the
// deadtime shortens the leg's pulse by its own length, and while the current
// flows into the leg, lengthens it; so each duty cycle is raised or lowered by
// `deadtime_dutycycle`, the effective deadtime divided by the PWM period.
// Below `current_threshold` in magnitude, the correction is proportional to
// the current instead: a small current does not recharge the leg's output
// capacitance within the deadtime and the error is smaller. A
// `current_threshold` of zero gives the full correction to any nonzero
// current. The results are clamped to [0, 1].
constexpr three_phase<unsigned_pu_f32>
compensate_deadtime(three_phase<unsigned_pu_f32> const& dutycycles,
                    current_abc const& currents,
                    unsigned_pu_f32 deadtime_dutycycle,
                    float current_threshold)
{
  float const inv_threshold =
      current_threshold > 0.f ? 1.f / current_threshold : 0.f;

  auto const compensate = [&](unsigned_pu_f32 duty, float current) {
    float const k = current_threshold > 0.f
                      ? std::clamp(current * inv_threshold, -1.f, 1.f)
                      : emb::sgn<float>(current);
    return unsigned_pu_f32{duty.value() + k * deadtime_dutycycle.value()};
  };

  return {.a = compensate(dutycycles.a, currents.a),
          .b = compensate(dutycycles.b, currents.b),
          .c = compensate(dutycycles.c, currents.c)};
}

} // namespace foc
} // namespace emb
