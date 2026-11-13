#pragma once

#include <emb/foc/types.hpp>
#include <emb/units/speed.hpp>

namespace emb {
namespace foc {

constexpr voltage_dq dq_feedforward_voltage(current_dq const& Imeas,
                                           some_motor auto const& motor,
                                           emb::units::eradps_f32 speed)
{
  float const Ld = motor.Ld;
  float const Lq = motor.Lq;
  float const Psi = motor.Psi;
  float const omega = speed.value;
  return {
      .d = -omega * Lq * Imeas.q,
      .q = omega * (Ld * Imeas.d + Psi),
  };
}

} // namespace foc
} // namespace emb
