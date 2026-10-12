#pragma once

#include <emb/foc/types.hpp>
#include <emb/math.hpp>

namespace emb {
namespace foc {

template<typename Q>
constexpr vec_polar<Q> to_polar(vec_ab<Q> x)
{
  return {.mag = emb::sqrt(x.alpha * x.alpha + x.beta * x.beta),
          .theta = emb::atan2(x.beta, x.alpha)};
}

} // namespace foc
} // namespace emb
