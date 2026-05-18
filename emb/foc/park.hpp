#pragma once

#include <emb/foc/types.hpp>

namespace emb {
namespace foc {

constexpr vec_dq park_transform(vec_ab v_ab, float sine, float cosine) {
  return {
      .d = (v_ab.alpha * cosine) + (v_ab.beta * sine),
      .q = (v_ab.beta * cosine) - (v_ab.alpha * sine)
  };
}

constexpr vec_ab invpark_transform(vec_dq v_dq, float sine, float cosine) {
  return {
      .alpha = (v_dq.d * cosine) - (v_dq.q * sine),
      .beta = (v_dq.q * cosine) + (v_dq.d * sine)
  };
}

} // namespace foc
} // namespace emb
