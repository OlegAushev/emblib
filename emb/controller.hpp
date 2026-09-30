#pragma once

#include <emb/units.hpp>

#include <algorithm>
#include <concepts>
#include <utility>

namespace emb {

enum class controller_action {
  reverse,
  direct
};

namespace detail {

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

template<std::floating_point T, controller_action Action>
class p_controller {
public:
  using value_type = T;
protected:
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

  constexpr void set_lower_limit(value_type value)
  {
    u_min_ = value;
  }

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

template<std::floating_point T>
struct pi_controller_params {
  T Kp;
  T Ki;
  T dt;
  T u_min;
  T u_max;
};

namespace antiwindup {

template<typename S, typename T>
concept some_scheme =
    requires(S& s, S const& cs, pi_controller_params<T> const& p, T e) {
      { s.push(p, e) } -> std::same_as<T>;
      { cs.integral() } -> std::same_as<T>;
      s.reset();
    };

template<std::floating_point T>
class backcalculation {
public:
  using value_type = T;
private:
  value_type Kb_;
  value_type I_{0};
public:
  constexpr explicit backcalculation(value_type kb) : Kb_(kb) {}

  constexpr value_type push(pi_controller_params<value_type> const& p,
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

template<std::floating_point T>
class clamping {
public:
  using value_type = T;
private:
  value_type I_{0};
public:
  constexpr value_type push(pi_controller_params<value_type> const& p,
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
  constexpr pi_controller(value_type kp,
                          value_type ki,
                          units::sec<value_type> timestep,
                          value_type lower_limit,
                          value_type upper_limit,
                          antiwindup_type aw = {})
      : params_{kp, ki, timestep.value(), lower_limit, upper_limit},
        aw_{std::move(aw)}
  {
  }

  constexpr void push(value_type ref, value_type meas)
  {
    u_ = aw_.push(params_, detail::error<Action>(ref, meas));
  }

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

  constexpr void set_lower_limit(value_type value)
  {
    params_.u_min = value;
  }

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
    params_.dt = value.value();
  }
};

template<std::floating_point T, controller_action Action>
using backcalculation_pi_controller =
    pi_controller<T, Action, antiwindup::backcalculation<T>>;

template<std::floating_point T, controller_action Action>
using clamping_pi_controller =
    pi_controller<T, Action, antiwindup::clamping<T>>;

} // namespace emb
