#pragma once

#include <emb/foc/types.hpp>

namespace emb {
namespace foc {

struct park_transform_fn {
  template<typename Q>
  constexpr vec_dq<Q>
  operator()(vec_ab<Q> v_ab, float sine, float cosine) const {
    return {
        .d = (v_ab.alpha * cosine) + (v_ab.beta * sine),
        .q = (v_ab.beta * cosine) - (v_ab.alpha * sine)
    };
  }
};

inline constexpr park_transform_fn park_transform{};

struct invpark_transform_fn {
  template<typename Q>
  constexpr vec_ab<Q>
  operator()(vec_dq<Q> v_dq, float sine, float cosine) const {
    return {
        .alpha = (v_dq.d * cosine) - (v_dq.q * sine),
        .beta = (v_dq.q * cosine) + (v_dq.d * sine)
    };
  }
};

inline constexpr invpark_transform_fn invpark_transform{};

} // namespace foc
} // namespace emb
