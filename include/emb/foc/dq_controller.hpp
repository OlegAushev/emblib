#pragma once

#include <emb/controller.hpp>
#include <emb/foc/pipe.hpp>
#include <emb/math.hpp>
#include <emb/motorcontrol.hpp>

#include <cmath>
#include <numbers>

namespace emb {
namespace foc {

using dq_controller_type =
    clamping_pi_controller<float, controller_policy::non_inverting>;

struct dq_input {
  vec_dq reference;
  vec_dq measured;
  float vDC;
};

class dq_control {
  dq_controller_type& iD_;
  dq_controller_type& iQ_;
  float vD_limit_factor_;
public:
  dq_control(
      dq_controller_type& iD,
      dq_controller_type& iQ,
      float vD_limit_factor = 1.0f
  )
      : iD_(iD), iQ_(iQ), vD_limit_factor_(vD_limit_factor) {}

  vec_dq operator()(dq_input const& in) {
    // D-axis controller
    float const vD_comp = 0.0f;
    float const vD_avail = in.vDC /
                           std::numbers::sqrt3_v<float> *
                           vD_limit_factor_;
    iD_.set_lower_limit(-vD_avail - vD_comp);
    iD_.set_upper_limit(vD_avail - vD_comp);
    iD_.push(in.reference.d, in.measured.d);
    float const vD = iD_.output() + vD_comp;

    // Q-axis controller with circular voltage limiting
    float const vQ_comp = 0.0f;
    float const vDC_over_sqrt3 = in.vDC / std::numbers::sqrt3_v<float>;
    if (std::fabs(vD) < vDC_over_sqrt3) {
      float vQ_avail;
      arm_sqrt_f32(vDC_over_sqrt3 * vDC_over_sqrt3 - vD * vD, &vQ_avail);
      iQ_.set_lower_limit(-vQ_avail - vQ_comp);
      iQ_.set_upper_limit(vQ_avail - vQ_comp);
    } else {
      iQ_.set_lower_limit(0.0f);
      iQ_.set_upper_limit(0.0f);
    }
    iQ_.push(in.reference.q, in.measured.q);
    float const vQ = iQ_.output() + vQ_comp;

    return {.d = vD, .q = vQ};
  }
};

} // namespace foc
} // namespace emb
