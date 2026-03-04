#pragma once

#include <emb/foc/types.hpp>

#include <array>
#include <numbers>

namespace emb {
namespace foc {

inline vec_ab clarke_transform(float a, float b, float c) {
  vec_ab retv;
  retv.alpha = a;
  retv.beta = (b - c) * std::numbers::inv_sqrt3_v<float>;
  return retv;
}

inline vec_ab clarke_transform(std::array<float, 3> const& v) {
  vec_ab retv;
  retv.alpha = v[0];
  retv.beta = (v[1] - v[2]) * std::numbers::inv_sqrt3_v<float>;
  return retv;
}

inline vec_ab clarke_transform(float a, float b) {
  vec_ab retv;
  retv.alpha = a;
  retv.beta = (a + 2 * b) * std::numbers::inv_sqrt3_v<float>;
  return retv;
}

inline std::array<float, 3> invclarke_transform(vec_ab v) {
  std::array<float, 3> retv;
  retv[0] = v.alpha;
  retv[1] = (-v.alpha + std::numbers::sqrt3_v<float> * v.beta) * 0.5f;
  retv[2] = (-v.alpha - std::numbers::sqrt3_v<float> * v.beta) * 0.5f;
  return retv;
}

struct clarke_fn {
  static constexpr vec_ab operator()(std::array<float, 3> const& phase) {
    return clarke_transform(phase);
  }
};

inline constexpr clarke_fn clarke;

} // namespace foc
} // namespace emb
