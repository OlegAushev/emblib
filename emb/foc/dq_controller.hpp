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

// A PI controller per axis, with Vcomp added to their outputs. The output
// limits are derived on every push from Vdc/√3, the linear range of svpwm:
// the d axis may take Vd_limit_factor of it, the q axis what is left of the
// circle. The gains start at zero and are set through d() and q().
class dq_controller {
public:
  using axis_type =
      clamping_pi_controller<float, controller_policy::non_inverting>;
private:
  axis_type d_;
  axis_type q_;
  unsigned_pu_f32 Vd_limit_factor_;
  voltage_dq out_{};
public:
  constexpr dq_controller(units::sec_f32 timestep,
                          unsigned_pu_f32 Vd_limit_factor)
      : d_(0.0f, 0.0f, timestep, 0.0f, 0.0f),
        q_(0.0f, 0.0f, timestep, 0.0f, 0.0f),
        Vd_limit_factor_(Vd_limit_factor)
  {
  }

  constexpr void push(current_dq const& Iref,
                      current_dq const& Imeas,
                      voltage_dq const& Vcomp,
                      float Vdc)
  {
    float const Vdc_over_sqrt3 = Vdc * std::numbers::inv_sqrt3_v<float>;

    // D-axis controller
    float const Vd_avail = Vdc_over_sqrt3 * Vd_limit_factor_.value();
    d_.set_lower_limit(-Vd_avail - Vcomp.d);
    d_.set_upper_limit(Vd_avail - Vcomp.d);
    d_.push(Iref.d, Imeas.d);
    float const Vd = d_.output() + Vcomp.d;

    // Q-axis controller
    if (std::fabs(Vd) < Vdc_over_sqrt3) {
      float const Vq_avail =
          emb::sqrt(Vdc_over_sqrt3 * Vdc_over_sqrt3 - Vd * Vd);
      q_.set_lower_limit(-Vq_avail - Vcomp.q);
      q_.set_upper_limit(Vq_avail - Vcomp.q);
    }
    else {
      q_.set_lower_limit(0.0f);
      q_.set_upper_limit(0.0f);
    }
    q_.push(Iref.q, Imeas.q);
    float const Vq = q_.output() + Vcomp.q;

    out_ = {.d = Vd, .q = Vq};
  }

  constexpr voltage_dq output() const
  {
    return out_;
  }

  constexpr void reset()
  {
    d_.reset();
    q_.reset();
    out_ = {};
  }

  constexpr void set_timestep(units::sec_f32 value)
  {
    d_.set_timestep(value);
    q_.set_timestep(value);
  }

  constexpr axis_type& d()
  {
    return d_;
  }

  constexpr axis_type const& d() const
  {
    return d_;
  }

  constexpr axis_type& q()
  {
    return q_;
  }

  constexpr axis_type const& q() const
  {
    return q_;
  }

  constexpr unsigned_pu_f32 Vd_limit_factor() const
  {
    return Vd_limit_factor_;
  }

  constexpr void set_Vd_limit_factor(unsigned_pu_f32 value)
  {
    Vd_limit_factor_ = value;
  }
};

} // namespace foc
} // namespace emb
