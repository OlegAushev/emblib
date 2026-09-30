#pragma once

#include <emb/units.hpp>

#include <algorithm>
#include <concepts>
#include <utility>

namespace emb {

// The scoped enumeration `controller_action` specifies the action of a
// feedback controller: with positive gains, a rise in the measurement drives
// the output of a reverse-acting controller down and that of a direct-acting
// one up. A plant whose output rises with its input needs reverse action; one
// whose output falls as its input rises needs direct action.
enum class controller_action {
  // Reverse action: the error is the reference minus the measurement.
  reverse,
  // Direct action: the error is the measurement minus the reference.
  direct
};

namespace detail {

// Returns the error of a controller with the action `Action` for the
// reference `ref` and the measurement `meas`, i.e. `ref - meas` if `Action` is
// `controller_action::reverse` and `meas - ref` if it is
// `controller_action::direct`.
template<controller_action Action, std::floating_point T>
constexpr T error(T ref, T meas)
{
  if constexpr (Action == controller_action::reverse) {
    return ref - meas;
  }
  else {
    return meas - ref;
  }
}

} // namespace detail

// The class template `p_controller` is a proportional controller with the
// action `Action`: `push(ref, meas)` sets the output to the product of the
// proportional gain and the error for the reference `ref` and the measurement
// `meas`, clamped to [`lower_limit()`, `upper_limit()`]. Construction and
// `reset()` set the output to zero, even if zero lies outside the limits.
template<std::floating_point T, controller_action Action>
class p_controller {
public:
  using value_type = T;
private:
  value_type Kp_;
  value_type u_min_;
  value_type u_max_;
  value_type u_;
public:
  constexpr p_controller(value_type kp,
                         value_type lower_limit,
                         value_type upper_limit)
      : Kp_(kp), u_min_(lower_limit), u_max_(upper_limit), u_(0)
  {
  }

  // Sets the output to the product of the proportional gain and the error for
  // the reference `ref` and the measurement `meas`, clamped to
  // [`lower_limit()`, `upper_limit()`]. The behavior is undefined if
  // `lower_limit()` is greater than `upper_limit()`, or if either limit or the
  // product is NaN.
  constexpr void push(value_type ref, value_type meas)
  {
    value_type const e = detail::error<Action>(ref, meas);
    value_type const u_unsat = Kp_ * e;
    u_ = std::clamp(u_unsat, u_min_, u_max_);
  }

  constexpr void reset()
  {
    u_ = 0;
  }

  constexpr value_type output() const
  {
    return u_;
  }

  // Sets the lower limit to `value`. The output is not clamped to it until the
  // next `push`.
  constexpr void set_lower_limit(value_type value)
  {
    u_min_ = value;
  }

  // Sets the upper limit to `value`. The output is not clamped to it until the
  // next `push`.
  constexpr void set_upper_limit(value_type value)
  {
    u_max_ = value;
  }

  constexpr value_type lower_limit() const
  {
    return u_min_;
  }

  constexpr value_type upper_limit() const
  {
    return u_max_;
  }

  constexpr void set_kp(value_type value)
  {
    Kp_ = value;
  }

  constexpr value_type kp() const
  {
    return Kp_;
  }
};

// The class template `pi_controller_params` holds the proportional gain, the
// integral gain, the timestep and the limits of a proportional-integral
// controller. `pi_controller` passes them, with the error, to its anti-windup
// scheme, which computes the output.
template<std::floating_point T>
struct pi_controller_params {
  T Kp;
  T Ki;
  T dt;
  T u_min;
  T u_max;
};

namespace antiwindup {

// The concept `some_scheme<S, T>` specifies that `S` is an anti-windup scheme
// for a proportional-integral controller with values of type `T`, i.e. a type
// whose objects hold the integral term of the controller and compute its
// output. Each scheme defines how the integral term is updated, in particular
// while the output is clamped to the limits.
//
// `S` models `some_scheme<S, T>` only if, given the error `e` and the
// parameters `p` such that `p.u_min <= p.u_max`, `s.step(p, e)` updates the
// integral term for one timestep and returns the output, clamped to
// [`p.u_min`, `p.u_max`]; `cs.integral()` returns the integral term; and
// `s.reset()` sets it to zero.
template<typename S, typename T>
concept some_scheme =
    requires(S& s, S const& cs, pi_controller_params<T> const& p, T e) {
      { s.step(p, e) } -> std::same_as<T>;
      { cs.integral() } -> std::same_as<T>;
      s.reset();
    };

// The class template `backcalculation` is an anti-windup scheme that, while the
// output is clamped, feeds the change that clamping makes to the output back
// into the integral term, scaled by the back-calculation gain, so that the
// unclamped output tracks the limit. The back-calculation gain is the
// reciprocal of the tracking time constant, in 1/s.
//
// If the product of the back-calculation gain and the timestep lies in (0, 2),
// then while the output stays clamped and everything but the integral term is
// constant, the unclamped output converges to the limit plus the integral gain
// times the error divided by the back-calculation gain. It converges without
// overshoot if the product lies in (0, 1]; an overshoot can take the output off
// the limit although the error is unchanged.
template<std::floating_point T>
class backcalculation {
public:
  using value_type = T;
private:
  value_type Kb_;
  value_type I_{0};
public:
  constexpr explicit backcalculation(value_type kb) : Kb_(kb) {}

  // Computes the output for the error `e` and the parameters `p`, i.e. the
  // unclamped output `p.Kp * e + integral()` clamped to [`p.u_min`, `p.u_max`],
  // then updates the integral term for one timestep and returns the output.
  // The integral term is incremented by `p.Ki * e * p.dt` plus `kb() * p.dt`
  // times the change that clamping makes to the output, i.e. the output minus
  // the unclamped output. The behavior is undefined if `p.u_min` is greater
  // than `p.u_max`, or if either limit or the unclamped output is NaN.
  constexpr value_type step(pi_controller_params<value_type> const& p,
                            value_type e)
  {
    value_type const u_unsat = p.Kp * e + I_;
    value_type const u = std::clamp(u_unsat, p.u_min, p.u_max);
    I_ += p.Ki * e * p.dt + Kb_ * (u - u_unsat) * p.dt;
    return u;
  }

  constexpr value_type integral() const
  {
    return I_;
  }

  constexpr void reset()
  {
    I_ = 0;
  }

  constexpr value_type kb() const
  {
    return Kb_;
  }

  constexpr void set_kb(value_type value)
  {
    Kb_ = value;
  }
};

// The class template `clamping` is an anti-windup scheme that, while the output
// is clamped, stops the integration if it would drive the unclamped output
// further past the limit (conditional integration), and that clamps the
// integral term to the limits at every `step`. The integral term is initially
// zero.
//
// Whether the integration would drive the unclamped output further past the
// limit is decided from the sign of the error alone, which assumes a
// non-negative integral gain. With a negative integral gain, the integration
// stops exactly when it would bring the unclamped output back toward the limit,
// and continues while it would drive the unclamped output further past, until
// the integral term reaches the limit.
template<std::floating_point T>
class clamping {
public:
  using value_type = T;
private:
  value_type I_{0};
public:
  // Computes the output for the error `e` and the parameters `p`, i.e. the
  // unclamped output `p.Kp * e + integral()` clamped to [`p.u_min`, `p.u_max`],
  // then updates the integral term for one timestep and returns the output.
  // Unless the unclamped output is greater than `p.u_max` and `e` is positive,
  // or less than `p.u_min` and `e` is negative, the integral term is
  // incremented by `p.Ki * e * p.dt`. Whether incremented or not, the integral
  // term is then clamped to [`p.u_min`, `p.u_max`]. The behavior is undefined
  // if `p.u_min` is greater than `p.u_max`, or if either limit, the unclamped
  // output or the incremented integral term is NaN.
  constexpr value_type step(pi_controller_params<value_type> const& p,
                            value_type e)
  {
    value_type const u_unsat = p.Kp * e + I_;
    value_type const u = std::clamp(u_unsat, p.u_min, p.u_max);
    bool const winding_up = (e * (u_unsat - u)) > 0;
    if (!winding_up) {
      I_ += p.Ki * e * p.dt;
    }
    I_ = std::clamp(I_, p.u_min, p.u_max);
    return u;
  }

  constexpr value_type integral() const
  {
    return I_;
  }

  constexpr void reset()
  {
    I_ = 0;
  }
};

} // namespace antiwindup

// The class template `pi_controller` is a proportional-integral controller
// with the action `Action` and an anti-windup scheme of type `AntiWindup`,
// which holds the integral term and computes the output: `push(ref, meas)`
// sets the output to the value that the scheme computes from the error for
// the reference `ref` and the measurement `meas`, clamped to
// [`lower_limit()`, `upper_limit()`], and has the scheme update the integral
// term for one timestep. The timestep is the period at which `push` is called.
// Construction and `reset()` set the output to zero, even if zero lies outside
// the limits.
//
// The scheme, e.g. `antiwindup::backcalculation` or `antiwindup::clamping`,
// defines how the output is computed and how the integral term is updated, in
// particular while the output is clamped.
template<std::floating_point T,
         controller_action Action,
         antiwindup::some_scheme<T> AntiWindup>
class pi_controller {
public:
  using value_type = T;
  using antiwindup_type = AntiWindup;
private:
  pi_controller_params<value_type> params_;
  antiwindup_type aw_;
  value_type u_{0};
public:
  // Constructs a controller whose output is zero, with the proportional gain
  // `kp`, the integral gain `ki`, in units of the proportional gain per second,
  // the timestep `timestep`, the lower limit `lower_limit`, the upper limit
  // `upper_limit` and the anti-windup scheme `aw`. The integral term is that of
  // `aw`. `aw` can be omitted only for a scheme that can be copy-initialized
  // from `{}`, e.g. `antiwindup::clamping` but not
  // `antiwindup::backcalculation`.
  constexpr pi_controller(value_type kp,
                          value_type ki,
                          units::sec<value_type> timestep,
                          value_type lower_limit,
                          value_type upper_limit,
                          antiwindup_type aw = {})
      : params_{kp, ki, timestep.value, lower_limit, upper_limit},
        aw_{std::move(aw)}
  {
  }

  // Sets the output to the value that the anti-windup scheme computes from the
  // error for the reference `ref` and the measurement `meas`, clamped to
  // [`lower_limit()`, `upper_limit()`], and has the scheme update the integral
  // term for one timestep. The behavior is undefined if `lower_limit()` is
  // greater than `upper_limit()`, or in any other case in which the scheme's
  // `step` has undefined behavior.
  constexpr void push(value_type ref, value_type meas)
  {
    u_ = aw_.step(params_, detail::error<Action>(ref, meas));
  }

  // Sets the output to zero and resets the anti-windup scheme as if by
  // `antiwindup().reset()`, which sets the integral term to zero. The gains,
  // the timestep and the limits are unchanged.
  constexpr void reset()
  {
    aw_.reset();
    u_ = 0;
  }

  constexpr value_type output() const
  {
    return u_;
  }

  constexpr value_type integral() const
  {
    return aw_.integral();
  }

  constexpr antiwindup_type& antiwindup()
  {
    return aw_;
  }

  constexpr antiwindup_type const& antiwindup() const
  {
    return aw_;
  }

  // Sets the lower limit to `value`. The output is not clamped to it until the
  // next `push`.
  constexpr void set_lower_limit(value_type value)
  {
    params_.u_min = value;
  }

  // Sets the upper limit to `value`. The output is not clamped to it until the
  // next `push`.
  constexpr void set_upper_limit(value_type value)
  {
    params_.u_max = value;
  }

  constexpr value_type lower_limit() const
  {
    return params_.u_min;
  }

  constexpr value_type upper_limit() const
  {
    return params_.u_max;
  }

  constexpr void set_kp(value_type value)
  {
    params_.Kp = value;
  }

  // Sets the integral gain to `value`, in units of the proportional gain per
  // second.
  constexpr void set_ki(value_type value)
  {
    params_.Ki = value;
  }

  constexpr value_type kp() const
  {
    return params_.Kp;
  }

  constexpr value_type ki() const
  {
    return params_.Ki;
  }

  constexpr void set_timestep(units::sec<value_type> value)
  {
    params_.dt = value.value;
  }
};

// `backcalculation_pi_controller<T, Action>` is a `pi_controller` with the
// action `Action` and an anti-windup scheme of type
// `antiwindup::backcalculation<T>`.
template<std::floating_point T, controller_action Action>
using backcalculation_pi_controller =
    pi_controller<T, Action, antiwindup::backcalculation<T>>;

// `clamping_pi_controller<T, Action>` is a `pi_controller` with the action
// `Action` and an anti-windup scheme of type `antiwindup::clamping<T>`.
template<std::floating_point T, controller_action Action>
using clamping_pi_controller =
    pi_controller<T, Action, antiwindup::clamping<T>>;

} // namespace emb
