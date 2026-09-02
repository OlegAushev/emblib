#pragma once

#include <emb/foc/types.hpp>
#include <emb/math.hpp>

namespace emb {
namespace foc {

struct to_polar_fn {
  template<typename Q>
  constexpr vec_polar<Q> operator()(vec_ab<Q> arg) const
  {
    return {.mag = emb::sqrt(arg.alpha * arg.alpha + arg.beta * arg.beta),
            .theta = emb::atan2(arg.beta, arg.alpha)};
  }
};

inline constexpr to_polar_fn to_polar{};

} // namespace foc
} // namespace emb
