#pragma once

#include <emb/math/numeric.hpp>

#include <concepts>
#include <cstdint>
#include <numbers>

namespace emb {

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
