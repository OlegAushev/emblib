#pragma once

#include <emb/foc/types.hpp>

namespace emb {
namespace foc {

template<typename Q>
constexpr vec_dq<Q> park_transform(vec_ab<Q> x, sin_cos theta)
{
  return {.d = (x.alpha * theta.cosine) + (x.beta * theta.sine),
          .q = (x.beta * theta.cosine) - (x.alpha * theta.sine)};
}

template<typename Q>
constexpr vec_ab<Q> invpark_transform(vec_dq<Q> x, sin_cos theta)
{
  return {.alpha = (x.d * theta.cosine) - (x.q * theta.sine),
          .beta = (x.q * theta.cosine) + (x.d * theta.sine)};
}

} // namespace foc
} // namespace emb
