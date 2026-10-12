#pragma once

#include <emb/foc/types.hpp>

namespace emb {
namespace foc {

template<typename Q>
constexpr vec_dq<Q> park_transform(vec_ab<Q> v_ab, sin_cos theta)
{
  return {.d = (v_ab.alpha * theta.cosine) + (v_ab.beta * theta.sine),
          .q = (v_ab.beta * theta.cosine) - (v_ab.alpha * theta.sine)};
}

template<typename Q>
constexpr vec_ab<Q> invpark_transform(vec_dq<Q> v_dq, sin_cos theta)
{
  return {.alpha = (v_dq.d * theta.cosine) - (v_dq.q * theta.sine),
          .beta = (v_dq.q * theta.cosine) + (v_dq.d * theta.sine)};
}

} // namespace foc
} // namespace emb
