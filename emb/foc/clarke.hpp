#pragma once

#include <emb/foc/types.hpp>

#include <numbers>

namespace emb {
namespace foc {

template<typename Q>
constexpr vec_ab<Q> clarke_transform(vec_abc<Q> const& arg)
{
  return {.alpha = arg.a,
          .beta = (arg.b - arg.c) * std::numbers::inv_sqrt3_v<float>};
}

template<typename Q>
constexpr vec_abc<Q> invclarke_transform(vec_ab<Q> arg)
{
  return {.a = arg.alpha,
          .b = (-arg.alpha + std::numbers::sqrt3_v<float> * arg.beta) * 0.5f,
          .c = (-arg.alpha - std::numbers::sqrt3_v<float> * arg.beta) * 0.5f};
}

} // namespace foc
} // namespace emb
