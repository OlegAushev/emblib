#pragma once

#include <emb/controller.hpp>
#include <emb/foc/types.hpp>
#include <emb/math.hpp>
#include <emb/pipe.hpp>

#include <cmath>
#include <numbers>

namespace emb {
namespace foc {

class dq_compensation : public pipe::pipeable<dq_compensation> {
  float Ld_;
  float Lq_;
  float Psi_;
  float omega_;
public:
  dq_compensation(some_motor auto const& motor, emb::units::eradps_f32 speed)
      : Ld_(motor.Ld), Lq_(motor.Lq), Psi_(motor.Psi), omega_(speed.value())
  {
  }

  constexpr voltage_dq operator()(current_dq const& Imeas) const
  {
    return {
        .d = -omega_ * Lq_ * Imeas.q,
        .q = omega_ * (Ld_ * Imeas.d + Psi_),
    };
  }
};

using dq_controller_type =
    clamping_pi_controller<float, controller_policy::non_inverting>;

class dq_control : public pipe::pipeable<dq_control> {
  dq_controller_type& Id_;
  dq_controller_type& Iq_;
  current_dq Iref_;
  voltage_dq Vcomp_;
  float Vdc_;
  emb::unsigned_pu_f32 Vd_limit_factor_;
public:
  dq_control(dq_controller_type& Id,
             dq_controller_type& Iq,
             current_dq Iref,
             voltage_dq Vcomp,
             float Vdc,
             emb::unsigned_pu_f32 Vd_limit_factor)
      : Id_(Id),
        Iq_(Iq),
        Iref_(Iref),
        Vcomp_(Vcomp),
        Vdc_(Vdc),
        Vd_limit_factor_(Vd_limit_factor)
  {
  }

  // const: the step does not own the controllers, it drives them
  constexpr voltage_dq operator()(current_dq const& Imeas) const
  {
    // D-axis controller
    float const Vd_avail = Vdc_
                         / std::numbers::sqrt3_v<float>
                         * Vd_limit_factor_.value();
    Id_.set_lower_limit(-Vd_avail - Vcomp_.d);
    Id_.set_upper_limit(Vd_avail - Vcomp_.d);
    Id_.push(Iref_.d, Imeas.d);
    float const Vd = Id_.output() + Vcomp_.d;

    // Q-axis controller
    float const Vdc_over_sqrt3 = Vdc_ / std::numbers::sqrt3_v<float>;
    if (std::fabs(Vd) < Vdc_over_sqrt3) {
      float const Vq_avail = emb::sqrt(Vdc_over_sqrt3 * Vdc_over_sqrt3
                                       - Vd * Vd);
      Iq_.set_lower_limit(-Vq_avail - Vcomp_.q);
      Iq_.set_upper_limit(Vq_avail - Vcomp_.q);
    }
    else {
      Iq_.set_lower_limit(0.0f);
      Iq_.set_upper_limit(0.0f);
    }
    Iq_.push(Iref_.q, Imeas.q);
    float const Vq = Iq_.output() + Vcomp_.q;

    return {.d = Vd, .q = Vq};
  }
};

} // namespace foc
} // namespace emb
