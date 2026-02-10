#pragma once

#include <emb/units.hpp>

#include <algorithm>
#include <concepts>
#include <limits>

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

} // namespace controller_policy

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

template<std::floating_point T>
class pi_controller_base {
public:
  using value_type = T;
protected:
  value_type kp_;
  value_type ki_;
  units::sec<value_type> ts_;
  value_type out_i_;
  value_type lower_limit_;
  value_type upper_limit_;
  value_type out_;
public:
  constexpr pi_controller_base(
      value_type kp,
      value_type ki,
      units::sec<value_type> ts,
      value_type lower_limit,
      value_type upper_limit
  )
      : kp_(kp),
        ki_(ki),
        ts_(ts),
        out_i_(0),
        lower_limit_(lower_limit),
        upper_limit_(upper_limit),
        out_(0) {}

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

  constexpr void set_ki(value_type value) {
    ki_ = value;
  }

  constexpr value_type kp() const {
    return kp_;
  }

  constexpr value_type ki() const {
    return ki_;
  }

  constexpr value_type integral() const {
    return out_i_;
  }

  constexpr void set_sampling_period(units::sec<value_type> value) {
    ts_ = value;
  }
};

template<std::floating_point T, typename Policy>
class backcalc_pi_controller : public pi_controller_base<T> {
public:
  using value_type = T;
  using base_type = pi_controller_base<T>;
protected:
  using base_type::kp_;
  using base_type::ki_;
  using base_type::ts_;
  using base_type::out_i_;
  using base_type::lower_limit_;
  using base_type::upper_limit_;
  using base_type::out_;

  value_type kc_; // anti-windup gain
public:
  constexpr backcalc_pi_controller(
      value_type kp,
      value_type ki,
      units::sec<value_type> ts,
      value_type kc,
      value_type lower_limit,
      value_type upper_limit
  )
      : base_type(kp, ki, ts, lower_limit, upper_limit), kc_(kc) {}

  constexpr void push(value_type ref, value_type meas) {
    value_type error = Policy::template error<value_type>(ref, meas);
    value_type out = std::clamp(
        error * kp_ + out_i_,
        -std::numeric_limits<value_type>::max(),
        std::numeric_limits<value_type>::max()
    );
    out_ = std::clamp(out, lower_limit_, upper_limit_);
    value_type out_i = out_i_ + ki_ * ts_.value() * error - kc_ * (out - out_);
    out_i_ = std::clamp(
        out_i,
        -std::numeric_limits<value_type>::max(),
        std::numeric_limits<value_type>::max()
    );
  }

  constexpr void reset() {
    out_i_ = 0;
    out_ = 0;
  }
};

template<std::floating_point T, typename Policy>
class clamping_pi_controller : public pi_controller_base<T> {
public:
  using value_type = T;
  using base_type = pi_controller_base<T>;
protected:
  using base_type::kp_;
  using base_type::ki_;
  using base_type::ts_;
  using base_type::out_i_;
  using base_type::lower_limit_;
  using base_type::upper_limit_;
  using base_type::out_;

  value_type error_;
public:
  constexpr clamping_pi_controller(
      value_type kp,
      value_type ki,
      units::sec<value_type> ts,
      value_type lower_limit,
      value_type upper_limit
  )
      : base_type(kp, ki, ts, lower_limit, upper_limit), error_(0) {}

  constexpr void push(value_type ref, value_type meas) {
    value_type error = Policy::template error<value_type>(ref, meas);
    value_type out_p = error * kp_;
    value_type out_i = (error + error_) * value_type(0.5) * ki_ * ts_.value() +
                       out_i_;
    error_ = error;
    value_type out = out_p + out_i;

    if (out > upper_limit_) {
      out_ = upper_limit_;
      if (out_p < upper_limit_) {
        out_i_ = upper_limit_ - out_p;
      }
    } else if (out < lower_limit_) {
      out_ = lower_limit_;
      if (out_p > lower_limit_) {
        out_i_ = lower_limit_ - out_p;
      }
    } else {
      out_ = out;
      out_i_ = out_i;
    }
  }

  constexpr void reset() {
    out_i_ = 0;
    error_ = 0;
    out_ = 0;
  }
};

} // namespace emb
