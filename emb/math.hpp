#pragma once

#include <emb/math/clamped.hpp>
#include <emb/math/saturation.hpp>
#include <emb/math/scaled.hpp>
#include <emb/math/trigonometric.hpp>

#include <algorithm>
#include <bit>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <numbers>
#include <numeric>

#ifdef __arm__
extern "C" {
#include "arm_math.h"
}
#endif

namespace emb {

// Computes the sine of `x` (measured in radians) with the target's run-time
// implementation: `arm_sin_f32` from CMSIS-DSP on 32-bit Arm, `std::sin` on
// other targets. `arm_sin_f32` has an absolute error below 1.9e-5 for
// |`x`| <= 2pi and below 1e-4 for |`x`| <= 1000.
inline float builtin_sin(float x)
{
#ifdef __arm__
  return arm_sin_f32(x);
#else
  return std::sin(x);
#endif
}

// Computes the sine of `x` (measured in radians). Returns `builtin_sin(x)` at
// run time. During constant evaluation, returns `lookup_sin(x)`, whose absolute
// error is below 4e-7 for |`x`| <= 2pi and below 1e-4 for |`x`| <= 1000.
constexpr float sin(float x)
{
  if !consteval {
    return builtin_sin(x);
  }
  else {
    return lookup_sin(x);
  }
}

// Computes the cosine of `x` (measured in radians) with the target's run-time
// implementation: `arm_cos_f32` from CMSIS-DSP on 32-bit Arm, `std::cos` on
// other targets. `arm_cos_f32` has an absolute error below 1.9e-5 for
// |`x`| <= 2pi and below 1.1e-4 for |`x`| <= 1000.
inline float builtin_cos(float x)
{
#ifdef __arm__
  return arm_cos_f32(x);
#else
  return std::cos(x);
#endif
}

// Computes the cosine of `x` (measured in radians). Returns `builtin_cos(x)` at
// run time. During constant evaluation, returns `lookup_cos(x)`, whose absolute
// error is below 5e-7 for |`x`| <= 2pi and below 1e-4 for |`x`| <= 1000.
constexpr float cos(float x)
{
  if !consteval {
    return builtin_cos(x);
  }
  else {
    return lookup_cos(x);
  }
}

// Computes the arc tangent of `y`/`x`, in [-pi, pi], using the signs of `y` and
// `x` to determine the quadrant, with the target's run-time implementation:
// `arm_atan2_f32` from CMSIS-DSP on 32-bit Arm, `std::atan2` on other targets.
// `arm_atan2_f32` has an absolute error below 4.5e-7 for finite `y` and `x`
// that are not both zero. If both are zero, the result is zero on 32-bit Arm.
inline float builtin_atan2(float y, float x)
{
#ifdef __arm__
  float ret = 0.0f;
  arm_atan2_f32(y, x, &ret);
  return ret;
#else
  return std::atan2(y, x);
#endif
}

// Computes the arc tangent of `y`/`x`, in [-pi, pi], using the signs of `y` and
// `x` to determine the quadrant. Returns `builtin_atan2(y, x)` at run time.
// During constant evaluation, returns `fast_atan2(y, x)`.
constexpr float atan2(float y, float x)
{
  if !consteval {
    return builtin_atan2(y, x);
  }
  else {
    return fast_atan2(y, x);
  }
}

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

// Computes the floating-point remainder of the division `x / y` exactly, with
// the same result as `std::fmod(x, y)`, including the sign of a zero result.
// Returns NaN if `x` is NaN or infinite or if `y` is zero or NaN.
// Logarithmic in |`x`| / |`y`|.
template<std::floating_point T>
consteval T fmod_trivial(T x, T y)
{
  T const ax = x < 0 ? -x : x;
  T const ay = y < 0 ? -y : y;
  if (!(ax <= std::numeric_limits<T>::max()) || !(ay > 0)) {
    return std::numeric_limits<T>::quiet_NaN();
  }
  if (ax < ay) {
    return x;
  }
  T d = ay;
  while (d <= ax - d) {
    d *= 2;
  }
  T r = ax;
  for (; d >= ay; d /= 2) {
    if (r >= d) {
      r -= d;
    }
  }
  return x < 0 ? -r : r;
}

// Computes the floating-point remainder of the division `x / y`. Returns
// `std::fmod(x, y)` at run time. During constant evaluation, returns
// `fmod_trivial(x, y)`, which gives the same result.
template<std::floating_point T>
constexpr T fmod(T x, T y)
{
  if !consteval {
    return std::fmod(x, y);
  }
  else {
    return fmod_trivial(x, y);
  }
}

// Returns the sign of `v` as `T`: -1, 0 or +1.
//
// Requires only `V{0}` and `<`, so any ordered type works; zero, negative
// zero and NaN all give 0. `T` comes first so the result type can be named
// while `V` is deduced, e.g. `sgn<float>(v)`.
template<typename T = int, typename V>
constexpr T sgn(V v)
{
  return static_cast<T>((V{0} < v) - (v < V{0}));
}

// Checks whether `n` is even.
constexpr bool iseven(std::integral auto n)
{
  return n % 2 == 0;
}

// Checks whether `n` is odd.
constexpr bool isodd(std::integral auto n)
{
  return !iseven(n);
}

// Checks whether `a` and `b` differ by less than `eps`, a tolerance stated in
// the units of `a` and `b`. Returns `false` if any argument is NaN.
template<typename T>
constexpr bool approx(T a, T b, T eps)
{
  return (a < b ? b - a : a - b) < eps;
}

// Converts the angle `deg` from degrees to radians.
template<std::floating_point T>
constexpr T to_rad(T deg)
{
  return deg * (std::numbers::pi_v<T> / T{180});
}

// Converts the angle `rad` from radians to degrees.
template<std::floating_point T>
constexpr T to_deg(T rad)
{
  return rad * (T{180} / std::numbers::pi_v<T>);
}

// Converts the mechanical speed `n`, in revolutions per minute, of a machine
// with `p` pole pairs to its electrical angular speed, in radians per second.
template<std::floating_point T, std::integral P>
constexpr T to_eradps(T n, P p)
{
  return static_cast<T>(p) * n * (2 * std::numbers::pi_v<T> / T{60});
}

// Converts the electrical angular speed `w`, in radians per second, of a
// machine with `p` pole pairs to its mechanical speed, in revolutions per
// minute. The behavior is undefined if `p` is zero.
template<std::floating_point T, std::integral P>
constexpr T to_rpm(T w, P p)
{
  return w * (T{60} / (2 * std::numbers::pi_v<T>)) / static_cast<T>(p);
}

// Normalizes the angle `x` (measured in radians) into [0, 2pi), where 2pi
// stands for `2 * std::numbers::pi_v<T>`. The result is exact for `x >= 0` and
// correctly rounded for negative `x`, except that a result that rounds to 2pi
// becomes zero; a NaN or infinite `x` gives NaN.
template<std::floating_point T>
constexpr T norm2pi(T x)
{
  constexpr T two_pi = 2 * std::numbers::pi_v<T>;
  x = emb::fmod(x, two_pi);
  if (x <= 0) { // `fmod` returns -0 for the negative multiples of 2pi
    x += two_pi;
    if (x >= two_pi) {
      // |x| was below half an ulp of 2pi, so the sum rounded onto the bound.
      x = T{0};
    }
  }
  return x;
}

// Normalizes the angle `x` (measured in radians) into [-pi, pi). Equivalent
// to `norm2pi(x + std::numbers::pi_v<T>) - std::numbers::pi_v<T>`, so even
// for `x` already in range the result is rounded: for `float` and
// |`x`| <= 2pi, it is within 2.4e-7 of the exact value.
template<std::floating_point T>
constexpr T normpi(T x)
{
  return norm2pi(x + std::numbers::pi_v<T>) - std::numbers::pi_v<T>;
}

// Approximates `norm2pi(x)`, returning a value in [0, 2pi). For `float`, the
// result differs from `norm2pi(x)`, modulo 2pi, by at most 4.8e-7 if
// |`x`| <= 2pi and by at most 7.6e-8 * |`x`| otherwise. The behavior is
// undefined if `x` is NaN or infinite or if |`x`| >= 1.3e10.
template<std::floating_point T>
constexpr T norm2pi_fast(T x)
{
  constexpr T two_pi = 2 * std::numbers::pi_v<T>;
  constexpr T inv_two_pi = 1 / (2 * std::numbers::pi_v<T>);

  T norm = x * inv_two_pi;
  norm -= static_cast<T>(static_cast<std::int32_t>(norm) - (norm < T{0}));
  if (norm >= T{1}) norm = T{0};
  return norm * two_pi;
}

// Approximates `normpi(x)`, returning a value in [-pi, pi). For `float`, the
// result differs from `normpi(x)`, modulo 2pi, by at most 4.8e-7 if
// |`x`| <= 2pi and by at most 1.1e-7 * |`x`| otherwise. The behavior is
// undefined if `x` is NaN or infinite or if |`x`| >= 1.3e10.
template<std::floating_point T>
constexpr T normpi_fast(T x)
{
  return norm2pi_fast(x + std::numbers::pi_v<T>) - std::numbers::pi_v<T>;
}

} // namespace emb
