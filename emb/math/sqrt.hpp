#pragma once

#include <bit>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdint>

#ifdef __arm__
extern "C" {
#include "arm_math.h"
}
#endif

namespace emb {

// Approximates 1/sqrt(`x`) with a relative error below 4.8e-6 for `x` in
// [`FLT_MIN`, `FLT_MAX`]. If `x` is NaN or less than `FLT_MIN`, an `assert`
// fails; if `NDEBUG` is defined, the behavior is undefined.
constexpr float fast_rsqrt(float x)
{
  assert(x >= FLT_MIN);

  float const x2 = x * 0.5f;

  auto i = std::bit_cast<std::uint32_t>(x);
  i = 0x5F3759DF - (i >> 1);
  float y = std::bit_cast<float>(i);

  y = y * (1.5f - (x2 * y * y));
  y = y * (1.5f - (x2 * y * y));

  return y;
}

// Computes 1/sqrt(`x`) as `1.0f` divided by the target's run-time square root:
// `arm_sqrt_f32` from CMSIS-DSP on 32-bit Arm, `std::sqrtf` on other targets.
// The result has a relative error below 9e-8 for `x` in (0, `FLT_MAX`]. For a
// negative or NaN `x`, the result is +inf on 32-bit Arm and NaN on other
// targets.
inline float builtin_rsqrt(float x)
{
#ifdef __arm__
  float ret;
  arm_sqrt_f32(x, &ret);
  return 1.0f / ret;
#else
  return 1.0f / std::sqrtf(x);
#endif
}

// Computes 1/sqrt(`x`). Returns `builtin_rsqrt(x)` at run time. During
// constant evaluation, returns `fast_rsqrt(x)`, whose relative error is below
// 4.8e-6 for `x` in [`FLT_MIN`, `FLT_MAX`].
constexpr float rsqrt(float x)
{
  if !consteval {
    return builtin_rsqrt(x);
  }
  else {
    return fast_rsqrt(x);
  }
}

// Approximates the square root of `x` with a relative error below 4.8e-6 for
// `x` in [`FLT_MIN`, `FLT_MAX`]. Returns zero if `x` is less than `FLT_MIN`,
// i.e. zero or subnormal. If `x` is NaN or negative, an `assert` fails; if
// `NDEBUG` is defined, the result is unspecified.
constexpr float fast_sqrt(float x)
{
  assert(x >= 0.0f);
  if (x < FLT_MIN) return 0.0f;
  return x * fast_rsqrt(x);
}

// Computes the square root of `x` with the target's run-time implementation:
// `arm_sqrt_f32` from CMSIS-DSP on 32-bit Arm, `std::sqrtf` on other targets.
// For a non-negative `x`, the result is correctly rounded. For a negative or
// NaN `x`, the result is zero on 32-bit Arm and NaN on other targets.
inline float builtin_sqrt(float x)
{
#ifdef __arm__
  float ret;
  arm_sqrt_f32(x, &ret);
  return ret;
#else
  return std::sqrtf(x);
#endif
}

// Computes the square root of `x`. Returns `builtin_sqrt(x)` at run time.
// During constant evaluation, returns `fast_sqrt(x)`, whose relative error is
// below 4.8e-6 for `x` in [`FLT_MIN`, `FLT_MAX`]; unlike at run time, the root
// of a perfect square need not be exact, e.g. `sqrt(4.0f)` is `1.9999913f`.
constexpr float sqrt(float x)
{
  if !consteval {
    return builtin_sqrt(x);
  }
  else {
    return fast_sqrt(x);
  }
}

} // namespace emb
