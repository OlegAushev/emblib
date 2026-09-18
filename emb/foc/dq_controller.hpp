#pragma once

#include <emb/controller.hpp>
#include <emb/foc/types.hpp>
#include <emb/math.hpp>

#include <cmath>
#include <numbers>

namespace emb {
namespace foc {

constexpr voltage_dq dq_compensation(current_dq const& Imeas,
                                     some_motor auto const& motor,
                                     emb::units::eradps_f32 speed)
{
  float const Ld = motor.Ld;
  float const Lq = motor.Lq;
  float const Psi = motor.Psi;
  float const omega = speed.value();
  return {
      .d = -omega * Lq * Imeas.q,
      .q = omega * (Ld * Imeas.d + Psi),
  };
}

using dq_controller_type =
    clamping_pi_controller<float, controller_policy::non_inverting>;

constexpr voltage_dq dq_control(dq_controller_type& Id,
                                dq_controller_type& Iq,
                                current_dq const& Iref,
                                current_dq const& Imeas,
                                voltage_dq const& Vcomp,
                                float Vdc,
                                emb::unsigned_pu_f32 Vd_limit_factor)
{
  // D-axis controller
  float const Vd_avail =
      Vdc / std::numbers::sqrt3_v<float> * Vd_limit_factor.value();
  Id.set_lower_limit(-Vd_avail - Vcomp.d);
  Id.set_upper_limit(Vd_avail - Vcomp.d);
  Id.push(Iref.d, Imeas.d);
  float const Vd = Id.output() + Vcomp.d;

  // Q-axis controller
  float const Vdc_over_sqrt3 = Vdc / std::numbers::sqrt3_v<float>;
  if (std::fabs(Vd) < Vdc_over_sqrt3) {
    float const Vq_avail = emb::sqrt(Vdc_over_sqrt3 * Vdc_over_sqrt3 - Vd * Vd);
    Iq.set_lower_limit(-Vq_avail - Vcomp.q);
    Iq.set_upper_limit(Vq_avail - Vcomp.q);
  }
  else {
    Iq.set_lower_limit(0.0f);
    Iq.set_upper_limit(0.0f);
  }
  Iq.push(Iref.q, Imeas.q);
  float const Vq = Iq.output() + Vcomp.q;

  return {.d = Vd, .q = Vq};
}

} // namespace foc
} // namespace emb
