#pragma once

#include <emb/foc/types.hpp>
#include <emb/math.hpp>

#include <algorithm>

namespace emb {
namespace foc {

namespace pwm_mode {

struct spwm {
  static constexpr float offset(float, float, float) {
    return 0.f;
  }
};

struct svpwm {
  static constexpr float offset(float Va, float Vb, float Vc) {
    auto const [mn, mx] = std::minmax({Va, Vb, Vc});
    return -0.5f * (mx + mn);
  }
};

struct dpwm1 {
  static constexpr float offset(float Va, float Vb, float Vc) {
    auto const [mn, mx] = std::minmax({Va, Vb, Vc});
    return (mx + mn > 0.f) ? (1.f - mx) : (-1.f - mn);
  }
};

struct dpwm0 {
  static constexpr float offset(float Va, float Vb, float Vc) {
    auto const [mn, mx] = std::minmax({Va, Vb, Vc});
    bool const clamp_low = (Va >= Vb && Vb >= Vc)
                        || (Vb >= Vc && Vc >= Va)
                        || (Vc >= Va && Va >= Vb);
    return clamp_low ? (-1.f - mn) : (1.f - mx);
  }
};

struct dpwm2 {
  static constexpr float offset(float Va, float Vb, float Vc) {
    auto const [mn, mx] = std::minmax({Va, Vb, Vc});
    bool const clamp_high = (Va >= Vb && Vb >= Vc)
                         || (Vb >= Vc && Vc >= Va)
                         || (Vc >= Va && Va >= Vb);
    return clamp_high ? (1.f - mx) : (-1.f - mn);
  }
};

struct dpwm3 {
  static constexpr float offset(float Va, float Vb, float Vc) {
    auto const [mn, mx] = std::minmax({Va, Vb, Vc});
    float const mid = Va + Vb + Vc - mx - mn;
    return (mid > 0.f) ? (1.f - mx) : (-1.f - mn);
  }
};

struct dpwmmin {
  static constexpr float offset(float Va, float Vb, float Vc) {
    return -1.f - std::min({Va, Vb, Vc});
  }
};

struct dpwmmax {
  static constexpr float offset(float Va, float Vb, float Vc) {
    return 1.f - std::max({Va, Vb, Vc});
  }
};

} // namespace pwm_mode

template<typename Mode>
constexpr std::array<emb::unsigned_pu, 3>
modulate(std::array<float, 3> const& Vs, float Vdc) {
  if (Vdc <= 0.f) {
    return {unsigned_pu{0.5f}, unsigned_pu{0.5f}, unsigned_pu{0.5f}};
  }

  // normalization: [−1, +1]
  float const inv = 2.f / Vdc;
  float const Va = Vs[0] * inv;
  float const Vb = Vs[1] * inv;
  float const Vc = Vs[2] * inv;

  // common-mode offset
  float const Voff = Mode::offset(Va, Vb, Vc);

  // duty cycles
  std::array<emb::unsigned_pu, 3> duty;
  duty[0] = emb::unsigned_pu{(Va + Voff + 1.f) * 0.5f};
  duty[1] = emb::unsigned_pu{(Vb + Voff + 1.f) * 0.5f};
  duty[2] = emb::unsigned_pu{(Vc + Voff + 1.f) * 0.5f};
  return duty;
}

} // namespace foc
} // namespace emb
