#pragma once

#include <emb/foc/types.hpp>

#include <numbers>

namespace emb {
namespace foc {

struct clarke_transform_fn {
  template<typename Q>
  constexpr vec_ab<Q> operator()(vec_abc<Q> const& arg) const {
    return {
        .alpha = arg.a,
        .beta = (arg.b - arg.c) * std::numbers::inv_sqrt3_v<float>
    };
  }
};

inline constexpr clarke_transform_fn clarke_transform{};

struct invclarke_transform_fn {
  template<typename Q>
  constexpr vec_abc<Q> operator()(vec_ab<Q> arg) const {
    return {
        .a = arg.alpha,
        .b = (-arg.alpha + std::numbers::sqrt3_v<float> * arg.beta) * 0.5f,
        .c = (-arg.alpha - std::numbers::sqrt3_v<float> * arg.beta) * 0.5f
    };
  }
};

inline constexpr invclarke_transform_fn invclarke_transform{};

} // namespace foc
} // namespace emb
