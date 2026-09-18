#pragma once

#include <emb/foc/types.hpp>
#include <emb/math.hpp>

namespace emb {
namespace foc {

template<typename Q>
constexpr vec_polar<Q> to_polar(vec_ab<Q> arg)
{
  return {.mag = emb::sqrt(arg.alpha * arg.alpha + arg.beta * arg.beta),
          .theta = emb::atan2(arg.beta, arg.alpha)};
}

} // namespace foc
} // namespace emb
