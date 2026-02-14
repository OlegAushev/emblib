#pragma once

#include <emb/foc/pipe.hpp>
#include <emb/math.hpp>
#include <emb/motorcontrol.hpp>

namespace emb {
namespace foc {

struct to_polar {
  static vec_alpha operator()(vec_alphabeta const& ab) {
    vec_alpha result;
    arm_sqrt_f32(ab.alpha * ab.alpha + ab.beta * ab.beta, &result.mag);
    arm_atan2_f32(ab.beta, ab.alpha, &result.theta);
    return result;
  }
};

} // namespace foc
} // namespace emb
