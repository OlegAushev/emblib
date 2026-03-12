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
  float omega_;
public:
  dq_compensation(some_motor auto const& motor, emb::units::eradps_f32 speed)
      : Ld_(motor.Ld), Lq_(motor.Lq), Psi_(motor.Psi), omega_(speed.value()) {}

  constexpr vec_dq operator()(vec_dq const& Imeas) const {
    return {
        .d = -omega_ * Lq_ * Imeas.q,
        .q = omega_ * (Ld_ * Imeas.d + Psi_),
    };
  }
};

using dq_controller_type =
    clamping_pi_controller<float, controller_policy::non_inverting>;

class dq_control {
  dq_controller_type& Id_;
  dq_controller_type& Iq_;
public:
  dq_control(dq_controller_type& Id, dq_controller_type& Iq)
      : Id_(Id), Iq_(Iq) {}

  constexpr vec_dq operator()(
      vec_dq const& Imeas,
      vec_dq Iref,
      vec_dq Vcomp,
      float Vdc,
      emb::unsigned_pu Vd_limit_factor
  ) {
    // D-axis controller
    float const Vd_avail = Vdc
                         / std::numbers::sqrt3_v<float>
                         * Vd_limit_factor.value();
    Id_.set_lower_limit(-Vd_avail - Vcomp.d);
    Id_.set_upper_limit(Vd_avail - Vcomp.d);
    Id_.push(Iref.d, Imeas.d);
    float const Vd = Id_.output() + Vcomp.d;

    // Q-axis controller
    float const Vdc_over_sqrt3 = Vdc / std::numbers::sqrt3_v<float>;
    if (std::fabs(Vd) < Vdc_over_sqrt3) {
      float const Vq_avail = emb::sqrt(
          Vdc_over_sqrt3 * Vdc_over_sqrt3 - Vd * Vd
      );
      Iq_.set_lower_limit(-Vq_avail - Vcomp.q);
      Iq_.set_upper_limit(Vq_avail - Vcomp.q);
    } else {
      Iq_.set_lower_limit(0.0f);
      Iq_.set_upper_limit(0.0f);
    }
    Iq_.push(Iref.q, Imeas.q);
    float const Vq = Iq_.output() + Vcomp.q;

    return {.d = Vd, .q = Vq};
  }
};

} // namespace foc
} // namespace emb
