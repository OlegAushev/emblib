#pragma once

#include <algorithm>
#include <concepts>
#include <float.h>

namespace emb {

namespace controller_policy {

struct non_inverting {
  template<std::floating_point T>
  static constexpr T error(T ref, T meas) {
    return ref - meas;
  }
};

struct inverting {
  template<std::floating_point T>
  static constexpr T error(T ref, T meas) {
    return meas - ref;
  }
};

}

enum class controller_logic {
  direct,
  inverse
};

template<std::floating_point T, typename Policy>
class p_controller {
public:
  using value_type = T;
protected:
  value_type kp_;
  value_type lower_limit_;
  value_type upper_limit_;
  value_type out_;
public:
  constexpr p_controller(
      value_type kp,
      value_type lower_limit,
      value_type upper_limit
  )
      : kp_(kp),
        lower_limit_(lower_limit),
        upper_limit_(upper_limit),
        out_(0) {}

  constexpr void push(value_type ref, value_type meas) {
    value_type out = kp_ * Policy::template error<value_type>(ref, meas);
    out_ = std::clamp(out, lower_limit_, upper_limit_);
  }

  constexpr void reset() {
    out_ = 0;
  }

  constexpr value_type output() const {
    return out_;
  }

  constexpr void set_lower_limit(value_type value) {
    lower_limit_ = value;
  }

  constexpr void set_upper_limit(value_type value) {
    upper_limit_ = value;
  }

  constexpr value_type lower_limit() const {
    return lower_limit_;
  }

  constexpr value_type upper_limit() const {
    return upper_limit_;
  }

  constexpr void set_kp(value_type value) {
    kp_ = value;
  }

  constexpr value_type kp() const {
    return kp_;
  }
};

template<controller_logic Logic>
class abstract_pi_controller {
protected:
  float kp_;          // proportional gain
  float ki_;          // integral gain
  float ts_;          // sampling period
  float out_i_;       // integrator sum;
  float lower_limit_; // output lower limit
  float upper_limit_; // output upper limit
  float out_;         // output;

  static float _error(float ref, float meas);
public:
  abstract_pi_controller(
      float kp, float ki, float ts, float lower_limit, float upper_limit)
      : kp_(kp),
        ki_(ki),
        ts_(ts),
        out_i_(0),
        lower_limit_(lower_limit),
        upper_limit_(upper_limit),
        out_(0) {}

  virtual ~abstract_pi_controller() {}

  virtual void push(float ref, float meas) = 0;

  virtual void reset() {
    out_i_ = 0;
    out_ = 0;
  }

  float output() const { return out_; }

  void set_lower_limit(float value) { lower_limit_ = value; }

  void set_upper_limit(float value) { upper_limit_ = value; }

  float lower_limit() const { return lower_limit_; }

  float upper_limit() const { return upper_limit_; }

  void set_kp(float value) { kp_ = value; }

  void set_ki(float value) { ki_ = value; }

  float kp() const { return kp_; }

  float ki() const { return ki_; }

  float integral() const { return out_i_; }

  void set_sampling_period(float value) { ts_ = value; }
};

template<>
inline float abstract_pi_controller<controller_logic::direct>::_error(
    float ref, float meas) {
  return ref - meas;
}

template<>
inline float abstract_pi_controller<controller_logic::inverse>::_error(
    float ref, float meas) {
  return meas - ref;
}

template<controller_logic Logic>
class backcalc_pi_controller : public abstract_pi_controller<Logic> {
protected:
  float kc_; // anti-windup gain
public:
  backcalc_pi_controller(float kp,
                         float ki,
                         float ts,
                         float kc,
                         float lower_limit,
                         float upper_limit)
      : abstract_pi_controller<Logic>(kp, ki, ts, lower_limit, upper_limit),
        kc_(kc) {}

  virtual void push(float ref, float meas) override {
    float error = abstract_pi_controller<Logic>::_error(ref, meas);
    float out = std::clamp(error * this->_kp + this->_out_i, -FLT_MAX, FLT_MAX);
    this->out_ = std::clamp(out, this->_lower_limit, this->_upper_limit);
    float out_i =
        this->out_i_ + this->ki_ * this->ts_ * error - kc_ * (out - this->out_);
    this->out_i_ = std::clamp(out_i, -FLT_MAX, FLT_MAX);
  }
};

template<controller_logic Logic>
class clamping_pi_controller : public abstract_pi_controller<Logic> {
protected:
  float error_;
public:
  clamping_pi_controller(
      float kp, float ki, float ts, float lower_limit, float upper_limit)
      : abstract_pi_controller<Logic>(kp, ki, ts, lower_limit, upper_limit),
        error_(0) {}

  virtual void push(float ref, float meas) override {
    float error = abstract_pi_controller<Logic>::_error(ref, meas);
    float out_p = error * this->kp_;
    float out_i =
        (error + error_) * 0.5f * this->ki_ * this->ts_ + this->out_i_;
    error_ = error;
    float out = out_p + out_i;

    if (out > this->upper_limit_) {
      this->out_ = this->upper_limit_;
      if (out_p < this->upper_limit_) {
        this->out_i_ = this->upper_limit_ - out_p;
      }
    } else if (out < this->lower_limit_) {
      this->out_ = this->lower_limit_;
      if (out_p > this->lower_limit_) {
        this->out_i_ = this->lower_limit_ - out_p;
      }
    } else {
      this->out_ = out;
      this->out_i_ = out_i;
    }
  }

  virtual void reset() override {
    this->out_i_ = 0;
    error_ = 0;
    this->out_ = 0;
  }
};

} // namespace emb
