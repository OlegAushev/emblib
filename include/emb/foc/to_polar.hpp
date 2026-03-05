#pragma once

#include <emb/foc/types.hpp>
#include <emb/math.hpp>

namespace emb {
namespace foc {

struct to_polar_fn {
  static vec_polar operator()(vec_ab const& arg) {
    vec_polar result;
    result.mag = emb::sqrtf(arg.alpha * arg.alpha + arg.beta * arg.beta);
    arm_atan2_f32(arg.beta, arg.alpha, &result.theta);
    return result;
  }
};

inline constexpr to_polar_fn to_polar;

} // namespace foc
} // namespace emb
