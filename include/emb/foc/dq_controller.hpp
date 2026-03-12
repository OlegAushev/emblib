#pragma once

#include <emb/controller.hpp>
#include <emb/foc/types.hpp>
#include <emb/math.hpp>

#include <cmath>
#include <numbers>

namespace emb {
namespace foc {

class dq_compensation {
  float Ld_;
  float Lq_;
  float Psi_;
  emb::units::eradps_f32 speed_;
public:
  dq_compensation(some_motor auto const& motor, emb::units::eradps_f32 speed)
      : Ld_(motor.Ld), Lq_(motor.Lq), Psi_(motor.Psi), speed_(speed) {}

  constexpr vec_dq operator()(vec_dq const& Imeas) const {
    return {
      .d = -speed_.value() * Lq_ * Imeas.q,
      .q =  speed_.value() * (Ld_ * Imeas.d + Psi_),
    };
  }
};

using dq_controller_type =
    clamping_pi_controller<float, controller_policy::non_inverting>;

class dq_control {
  vec_dq Iref_;
  vec_dq Vcomp_;
  float Vdc_;
  dq_controller_type& Id_;
  dq_controller_type& Iq_;
  float Vd_limit_factor_;
public:
  dq_control(
      vec_dq Iref,
      vec_dq Vcomp,
      float vDC,
      dq_controller_type& iD,
      dq_controller_type& iQ,
      float vD_limit_factor = 1.0f
  )
      : Iref_(Iref),
        Vcomp_(Vcomp),
        Vdc_(vDC),
        Id_(iD),
        Iq_(iQ),
        Vd_limit_factor_(vD_limit_factor) {}

  constexpr vec_dq operator()(vec_dq const& Imeas) {
    // D-axis controller
    float const Vd_comp = Vcomp_.d;
    float const Vd_avail = Vdc_ /
                           std::numbers::sqrt3_v<float> *
                           Vd_limit_factor_;
    Id_.set_lower_limit(-Vd_avail - Vd_comp);
    Id_.set_upper_limit(Vd_avail - Vd_comp);
    Id_.push(Iref_.d, Imeas.d);
    float const Vd = Id_.output() + Vd_comp;

    // Q-axis controller with circular voltage limiting
    float const Vq_comp = Vcomp_.q;
    float const Vdc_over_sqrt3 = Vdc_ / std::numbers::sqrt3_v<float>;
    if (std::fabs(Vd) < Vdc_over_sqrt3) {
      float const Vq_avail = emb::sqrt(
          Vdc_over_sqrt3 * Vdc_over_sqrt3 - Vd * Vd
      );
      Iq_.set_lower_limit(-Vq_avail - Vq_comp);
      Iq_.set_upper_limit(Vq_avail - Vq_comp);
    } else {
      Iq_.set_lower_limit(0.0f);
      Iq_.set_upper_limit(0.0f);
    }
    Iq_.push(Iref_.q, Imeas.q);
    float const Vq = Iq_.output() + Vq_comp;

    return {.d = Vd, .q = Vq};
  }
};

} // namespace foc
} // namespace emb
