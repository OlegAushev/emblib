#pragma once

#include <emb/foc/types.hpp>

namespace emb {
namespace foc {

template<typename Q>
constexpr vec_dq<Q> park_transform(vec_ab<Q> v_ab, float sine, float cosine)
{
  return {.d = (v_ab.alpha * cosine) + (v_ab.beta * sine),
          .q = (v_ab.beta * cosine) - (v_ab.alpha * sine)};
}

template<typename Q>
constexpr vec_ab<Q> invpark_transform(vec_dq<Q> v_dq, float sine, float cosine)
{
  return {.alpha = (v_dq.d * cosine) - (v_dq.q * sine),
          .beta = (v_dq.q * cosine) + (v_dq.d * sine)};
}

} // namespace foc
} // namespace emb
