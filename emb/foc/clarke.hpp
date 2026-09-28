#pragma once

#include <emb/foc/types.hpp>

#include <numbers>

namespace emb {
namespace foc {

// The scoped enumeration `clarke_inputs` specifies the form of the Clarke
// transform computed by `clarke_transform`. With `two`, it reads only the
// phases `a` and `b` and takes `c` to be `-(a + b)`; with `three`, it reads
// all three phases and discards the zero-sequence component `(a + b + c) / 3`.
enum class clarke_inputs { two, three };

// Computes the amplitude-invariant Clarke transform of the phase quantities
// `arg`: a balanced set of amplitude A becomes a vector of magnitude A in the
// stationary alpha-beta frame. If `N` is `clarke_inputs::three`, returns
// `alpha = (2 * a - b - c) / 3` and `beta = (b - c) / sqrt(3)`, i.e. discards
// the zero-sequence component `(a + b + c) / 3`. If `N` is
// `clarke_inputs::two`, returns `alpha = a` and `beta = (a + 2 * b) / sqrt(3)`
// without reading `c`, i.e. takes `c` to be `-(a + b)`. Both forms give the
// same vector, up to rounding, if `a + b + c == 0`.
template<clarke_inputs N, typename Q>
constexpr vec_ab<Q> clarke_transform(vec_abc<Q> const& arg)
{
  constexpr float inv_sqrt3 = std::numbers::inv_sqrt3_v<float>;
  if constexpr (N == clarke_inputs::two) {
    return {.alpha = arg.a, .beta = (arg.a + 2.f * arg.b) * inv_sqrt3};
  }
  else {
    return {.alpha = (2.f * arg.a - arg.b - arg.c) * (1.f / 3),
            .beta = (arg.b - arg.c) * inv_sqrt3};
  }
}

// Computes the amplitude-invariant inverse Clarke transform of `arg`, i.e.
// the phase quantities `a = alpha`, `b = (-alpha + sqrt(3) * beta) / 2` and
// `c = (-alpha - sqrt(3) * beta) / 2`. Up to rounding, they sum to zero, and
// for `arg` = `clarke_transform<clarke_inputs::three>(x)` the result is `x`
// without its zero-sequence component.
template<typename Q>
constexpr vec_abc<Q> invclarke_transform(vec_ab<Q> arg)
{
  return {.a = arg.alpha,
          .b = (-arg.alpha + std::numbers::sqrt3_v<float> * arg.beta) * 0.5f,
          .c = (-arg.alpha - std::numbers::sqrt3_v<float> * arg.beta) * 0.5f};
}

} // namespace foc
} // namespace emb
