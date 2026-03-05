#pragma once

#include <emb/foc/types.hpp>
#include <emb/math.hpp>

namespace emb {
namespace foc {

struct to_polar_fn {
  static constexpr vec_polar operator()(vec_ab const& arg) {
    return {
        .mag = emb::sqrt(arg.alpha * arg.alpha + arg.beta * arg.beta),
        .theta = emb::atan2(arg.beta, arg.alpha)
    };
  }
};

inline constexpr to_polar_fn to_polar;

} // namespace foc
} // namespace emb
