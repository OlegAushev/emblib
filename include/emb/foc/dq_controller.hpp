#pragma once

#include <emb/controller.hpp>
#include <emb/foc/types.hpp>
#include <emb/math.hpp>

#include <cmath>
#include <numbers>

namespace emb {
namespace foc {

using dq_controller_type =
    clamping_pi_controller<float, controller_policy::non_inverting>;

class dq_control {
  vec_dq reference_;
  float vDC_;
  dq_controller_type& iD_;
  dq_controller_type& iQ_;
  float vD_limit_factor_;
public:
  dq_control(
      vec_dq reference,
      float vDC,
      dq_controller_type& iD,
      dq_controller_type& iQ,
      float vD_limit_factor = 1.0f
  )
      : reference_(reference),
        vDC_(vDC),
        iD_(iD),
        iQ_(iQ),
        vD_limit_factor_(vD_limit_factor) {}

  constexpr vec_dq operator()(vec_dq const& measured) {
    // D-axis controller
    float const vD_comp = 0.0f;
    float const vD_avail = vDC_ /
                           std::numbers::sqrt3_v<float> *
                           vD_limit_factor_;
    iD_.set_lower_limit(-vD_avail - vD_comp);
    iD_.set_upper_limit(vD_avail - vD_comp);
    iD_.push(reference_.d, measured.d);
    float const vD = iD_.output() + vD_comp;

    // Q-axis controller with circular voltage limiting
    float const vQ_comp = 0.0f;
    float const vDC_over_sqrt3 = vDC_ / std::numbers::sqrt3_v<float>;
    if (std::fabs(vD) < vDC_over_sqrt3) {
      float const vQ_avail = emb::sqrtf(
          vDC_over_sqrt3 * vDC_over_sqrt3 - vD * vD
      );
      iQ_.set_lower_limit(-vQ_avail - vQ_comp);
      iQ_.set_upper_limit(vQ_avail - vQ_comp);
    } else {
      iQ_.set_lower_limit(0.0f);
      iQ_.set_upper_limit(0.0f);
    }
    iQ_.push(reference_.q, measured.q);
    float const vQ = iQ_.output() + vQ_comp;

    return {.d = vD, .q = vQ};
  }
};

} // namespace foc
} // namespace emb
