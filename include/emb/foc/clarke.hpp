#pragma once

#include <emb/foc/types.hpp>

#include <array>
#include <numbers>

namespace emb {
namespace foc {

constexpr vec_ab clarke_transform(std::array<float, 3> const& arg) {
  return {
      .alpha = arg[0],
      .beta = (arg[1] - arg[2]) * std::numbers::inv_sqrt3_v<float>
  };
}

// constexpr vec_ab clarke_transform(float a, float b, float c) {
//   return {
//       .alpha = a,
//       .beta = (b - c) * std::numbers::inv_sqrt3_v<float>
//   };
// }

// constexpr vec_ab clarke_transform(float a, float b) {
//   return {
//       .alpha = a,
//       .beta = (a + 2 * b) * std::numbers::inv_sqrt3_v<float>
//   };
// }

constexpr std::array<float, 3> invclarke_transform(vec_ab arg) {
  return {
      arg.alpha,
      (-arg.alpha + std::numbers::sqrt3_v<float> * arg.beta) * 0.5f,
      (-arg.alpha - std::numbers::sqrt3_v<float> * arg.beta) * 0.5f
  };
}

} // namespace foc
} // namespace emb
