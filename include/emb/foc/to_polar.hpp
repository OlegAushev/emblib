#pragma once

#include <emb/foc/types.hpp>
#include <emb/math.hpp>

namespace emb {
namespace foc {

struct to_polar_fn {
  static vec_polar operator()(vec_ab const& ab) {
    vec_polar result;
    arm_sqrt_f32(ab.alpha * ab.alpha + ab.beta * ab.beta, &result.mag);
    arm_atan2_f32(ab.beta, ab.alpha, &result.theta);
    return result;
  }
};

inline constexpr to_polar_fn to_polar;

} // namespace foc
} // namespace emb
