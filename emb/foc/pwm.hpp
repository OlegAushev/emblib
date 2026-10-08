#pragma once

#include <emb/foc/types.hpp>
#include <emb/math.hpp>

#include <algorithm>
#include <variant>

namespace emb {
namespace foc {

namespace pwm_mode {

struct spwm {
  static constexpr float offset(float, float, float)
  {
    return 0.f;
  }
};

struct svpwm {
  static constexpr float offset(float Va, float Vb, float Vc)
  {
    auto const [mn, mx] = std::minmax({Va, Vb, Vc});
    return -0.5f * (mx + mn);
  }
};

struct dpwmmin {
  static constexpr float offset(float Va, float Vb, float Vc)
  {
    return -1.f - std::min({Va, Vb, Vc});
  }
};

struct dpwmmax {
  static constexpr float offset(float Va, float Vb, float Vc)
  {
    return 1.f - std::max({Va, Vb, Vc});
  }
};

namespace detail {

constexpr bool in_sector_0_2_4(float Va, float Vb, float Vc)
{
  return (Va >= Vb && Vb >= Vc)
      || (Vb >= Vc && Vc >= Va)
      || (Vc >= Va && Va >= Vb);
}

} // namespace detail

struct dpwm0 {
  static constexpr float offset(float Va, float Vb, float Vc)
  {
    return detail::in_sector_0_2_4(Va, Vb, Vc) ? dpwmmin::offset(Va, Vb, Vc)
                                               : dpwmmax::offset(Va, Vb, Vc);
  }
};

struct dpwm1 {
  static constexpr float offset(float Va, float Vb, float Vc)
  {
    auto const [mn, mx] = std::minmax({Va, Vb, Vc});
    return (mx + mn > 0.f) ? (1.f - mx) : (-1.f - mn);
  }
};

struct dpwm2 {
  static constexpr float offset(float Va, float Vb, float Vc)
  {
    return detail::in_sector_0_2_4(Va, Vb, Vc) ? dpwmmax::offset(Va, Vb, Vc)
                                               : dpwmmin::offset(Va, Vb, Vc);
  }
};

struct dpwm3 {
  static constexpr float offset(float Va, float Vb, float Vc)
  {
    auto const [mn, mx] = std::minmax({Va, Vb, Vc});
    float const mid = Va + Vb + Vc - mx - mn;
    return (mid > 0.f) ? (1.f - mx) : (-1.f - mn);
  }
};

struct adaptive_dpwm {
  current_dq I;
  voltage_dq V;

  constexpr float offset(float Va, float Vb, float Vc) const
  {
    float const p = V.d * I.d + V.q * I.q;
    float const q = V.q * I.d - V.d * I.q;
    if (std::abs(q) <= 0.2679f * std::abs(p)) { // 15 deg
      return dpwm1::offset(Va, Vb, Vc);
    }
    return (p * q > 0.f) ? dpwm2::offset(Va, Vb, Vc)
                         : dpwm0::offset(Va, Vb, Vc);
  }
};

struct gdpwm {
  current_dq I;
  voltage_dq V;

  constexpr float offset(float Va, float Vb, float Vc) const
  {
    constexpr float inv_sqrt3 = std::numbers::inv_sqrt3_v<float>;
    float p = V.d * I.d + V.q * I.q;
    float q = V.q * I.d - V.d * I.q;
    if (std::abs(q) >= inv_sqrt3 * std::abs(p)) {
      return (p * q > 0.f) ? dpwm2::offset(Va, Vb, Vc)
                           : dpwm0::offset(Va, Vb, Vc);
    }

    if (p < 0.f) {
      p = -p;
      q = -q;
    }

    float const k = q * inv_sqrt3;
    float const Ra = p * Va + k * (Vb - Vc);
    float const Rb = p * Vb + k * (Vc - Va);
    float const Rc = p * Vc + k * (Va - Vb);
    auto const [rmn, rmx] = std::minmax({Ra, Rb, Rc});
    return (rmx + rmn > 0.f) ? dpwmmax::offset(Va, Vb, Vc)
                             : dpwmmin::offset(Va, Vb, Vc);
  }
};

} // namespace pwm_mode

template<typename T>
concept some_pwm_mode = requires (T const& mode, float v) {
  { mode.offset(v, v, v) } -> std::same_as<float>;
};

namespace detail {

template<some_pwm_mode Mode>
constexpr float
offset(Mode const& mode, float Va, float Vb, float Vc)
{
  return mode.offset(Va, Vb, Vc);
}

template<some_pwm_mode... Modes>
constexpr float
offset(std::variant<Modes...> const& mode, float Va, float Vb, float Vc)
{
  return mode.visit([&](auto const& m) { return m.offset(Va, Vb, Vc); });
}

} // namespace detail

template<typename Mode>
constexpr three_phase<emb::unsigned_pu_f32>
modulate(Mode const& mode, voltage_abc const& Vs, float Vdc)
{
  if (Vdc <= 0.f) {
    return {.a = unsigned_pu_f32{0.5f},
            .b = unsigned_pu_f32{0.5f},
            .c = unsigned_pu_f32{0.5f}};
  }

  // normalization: [−1, +1]
  float const inv = 2.f / Vdc;
  float const Va = Vs.a * inv;
  float const Vb = Vs.b * inv;
  float const Vc = Vs.c * inv;

  // common-mode offset
  float const Voff = detail::offset(mode, Va, Vb, Vc);

  // duty cycles
  return {.a = emb::unsigned_pu_f32{(Va + Voff + 1.f) * 0.5f},
          .b = emb::unsigned_pu_f32{(Vb + Voff + 1.f) * 0.5f},
          .c = emb::unsigned_pu_f32{(Vc + Voff + 1.f) * 0.5f}};
}

} // namespace foc
} // namespace emb
