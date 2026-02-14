#pragma once

#include <emb/controller.hpp>
#include <emb/foc/pipe.hpp>
#include <emb/math.hpp>
#include <emb/motorcontrol.hpp>

#include <cmath>
#include <numbers>

namespace emb {
namespace foc {

struct dq_input {
  vec_dq reference;
  vec_dq measured;
  float vDC;
};

class dq_controller {
  using controller_type =
      clamping_pi_controller<float, controller_policy::non_inverting>;
private:
  controller_type iD_;
  controller_type iQ_;
  float vD_limit_factor_{1.0f};
public:
  dq_controller(
      float iD_kP,
      float iD_kI,
      float iQ_kP,
      float iQ_kI,
      units::sec<float> ts,
      float lower_limit,
      float upper_limit
  )
      : iD_(iD_kP, iD_kI, ts, lower_limit, upper_limit),
        iQ_(iQ_kP, iQ_kI, ts, lower_limit, upper_limit) {}

  void set_vD_limit_factor(float factor) {
    vD_limit_factor_ = factor;
  }

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
      arm_sqrt_f32(in.vDC * in.vDC / 3.0f - vD * vD, &vQ_avail);
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

  controller_type const& iD() const {
    return iD_;
  }

  controller_type const& iQ() const {
    return iQ_;
  }

  void reset() {
    iD_.reset();
    iQ_.reset();
  }
};

} // namespace foc
} // namespace emb
