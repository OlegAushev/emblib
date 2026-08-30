#pragma once

#include <emb/math/clamped.hpp>
#include <emb/math/saturation.hpp>
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
#include <ratio>

#ifdef __arm__
extern "C" {
#include "arm_math.h"
}
#endif

namespace emb {

// ---- sin ----
inline float builtin_sin(float x) {
#ifdef __arm__
  return arm_sin_f32(x);
#endif
#ifdef __x86_64__
  return std::sin(x);
#endif
}

constexpr float sin(float x) {
  if !consteval {
    return builtin_sin(x);
  } else {
    return lookup_sin(x);
  }
}

// ---- cos ----
inline float builtin_cos(float x) {
#ifdef __arm__
  return arm_cos_f32(x);
#endif
#ifdef __x86_64__
  return std::cos(x);
#endif
}

constexpr float cos(float x) {
  if !consteval {
    return builtin_cos(x);
  } else {
    return lookup_cos(x);
  }
}

// ---- atan2 ----
inline float builtin_atan2(float y, float x) {
#ifdef __arm__
  float ret;
  arm_atan2_f32(y, x, &ret);
  return ret;
#endif
#ifdef __x86_64__
  return std::atan2(y, x);
#endif
}

constexpr float atan2(float y, float x) {
  if !consteval {
    return builtin_atan2(y, x);
  } else {
    return fast_atan2(y, x);
  }
}

// ---- rsqrt/sqrt ----
constexpr float fast_rsqrt(float arg) {
  assert(arg >= FLT_MIN);

  const float x2 = arg * 0.5f;

  auto i = std::bit_cast<std::uint32_t>(arg);
  i = 0x5f3759df - (i >> 1);
  float y = std::bit_cast<float>(i);

  y = y * (1.5f - (x2 * y * y));
  y = y * (1.5f - (x2 * y * y));

  return y;
}

inline float builtin_rsqrt(float arg) {
#ifdef __arm__
  float ret;
  arm_sqrt_f32(arg, &ret);
  return 1.0f / ret;
#endif
#ifdef __x86_64__
  return 1.0f / std::sqrtf(arg);
#endif
}

constexpr float rsqrt(float arg) {
  if !consteval {
    return builtin_rsqrt(arg);
  } else {
    return fast_rsqrt(arg);
  }
}

constexpr float fast_sqrt(float arg) {
  assert(arg >= 0.0f);
  if (arg < FLT_MIN) return 0.0f;
  return arg * fast_rsqrt(arg);
}

inline float builtin_sqrt(float arg) {
#ifdef __arm__
  float ret;
  arm_sqrt_f32(arg, &ret);
  return ret;
#endif
#ifdef __x86_64__
  return std::sqrtf(arg);
#endif
}

constexpr float sqrt(float arg) {
  if !consteval {
    return builtin_sqrt(arg);
  } else {
    return fast_sqrt(arg);
  }
}

// ---- fmod ----
template<std::floating_point T>
consteval T fmod_trivial(T x, T y) {
  return x - static_cast<T>(static_cast<long long>(x / y)) * y;
}

template<std::floating_point T>
constexpr T fmod(T x, T y) {
  if !consteval {
    return std::fmod(x, y);
  } else {
    return fmod_trivial(x, y);
  }
}

// ---- sgn ----
template<typename T = int, typename V>
constexpr T sgn(V v) {
  return T((V(0) < v) - (v < V(0)));
}

// ---- iseven ----
constexpr bool iseven(std::integral auto v) {
  return v % 2 == 0;
}

// ---- isodd ----
constexpr bool isodd(std::integral auto v) {
  return !iseven(v);
}

// ---- saturate_round ----
template<std::integral Int, std::floating_point Float>
constexpr Int saturate_round(Float num) {
  static_assert(
      sizeof(Int) < sizeof(long long) || std::is_signed_v<Int>,
      "u64 upper range is unreachable via llround"
  );
  constexpr bool fits_long = sizeof(Int) < sizeof(long)
                          || (sizeof(Int) == sizeof(long)
                              && std::is_signed_v<Int>);
  using Wide = std::conditional_t<fits_long, long, long long>;

  assert(!std::isnan(num));
  if (num >= static_cast<Float>(std::numeric_limits<Wide>::max())) {
    return std::numeric_limits<Int>::max();
  }
  if (num <= static_cast<Float>(std::numeric_limits<Wide>::min())) {
    return std::numeric_limits<Int>::min();
  }

  if constexpr (fits_long) {
    return emb::saturating_cast<Int>(std::lround(num));
  } else {
    return emb::saturating_cast<Int>(std::llround(num));
  }
}

// ---- quantize ----
template<std::integral Int, typename Step, std::floating_point Float>
  requires requires { Step::num; Step::den; }
constexpr Int quantize(Float num) {
  static_assert(Step::num > 0, "Step must be a positive ratio");
  constexpr Float scale = static_cast<Float>(Step::den)
                        / static_cast<Float>(Step::num);
  return saturate_round<Int>(num * scale);
}

// ---- dequantize ----
template<typename Step, std::floating_point Float = float, std::integral Int>
  requires requires { Step::num; Step::den; }
constexpr Float dequantize(Int num) {
  static_assert(Step::num > 0, "Step must be a positive ratio");
  constexpr Float step = static_cast<Float>(Step::num)
                       / static_cast<Float>(Step::den);
  return static_cast<Float>(num) * step;
}

// -----------------------------------------------------------------------------
template<std::floating_point T>
constexpr T to_rad(T deg) {
  return deg * (std::numbers::pi_v<T> / T{180});
}

template<std::floating_point T>
constexpr T to_deg(T rad) {
  return rad * (T{180} / std::numbers::pi_v<T>);
}

template<std::floating_point T, std::integral P>
constexpr T to_eradps(T n, P p) {
  return static_cast<T>(p) * n * (2 * std::numbers::pi_v<T> / T{60});
}

template<std::floating_point T, std::integral P>
constexpr T to_rpm(T w, P p) {
  return w * (T{60} / (2 * std::numbers::pi_v<T>)) / static_cast<T>(p);
}

template<std::floating_point T>
constexpr T norm2pi(T v) {
  constexpr T two_pi = 2 * std::numbers::pi_v<T>;
  v = emb::fmod(v, two_pi);
  if (v < 0) {
    v += two_pi;
  }
  return v;
}

template<std::floating_point T>
constexpr T normpi(T v) {
  return norm2pi(v + std::numbers::pi_v<T>) - std::numbers::pi_v<T>;
}

template<std::floating_point T>
constexpr T norm2pi_fast(T v) {
  constexpr T two_pi = 2 * std::numbers::pi_v<T>;
  constexpr T inv_two_pi = 1 / (2 * std::numbers::pi_v<T>);

  T norm = v * inv_two_pi;
  norm -= static_cast<T>(static_cast<std::int32_t>(norm) - (norm < T{0}));
  if (norm >= T{1}) norm -= T{1};
  return norm * two_pi;
}

template<std::floating_point T>
constexpr T normpi_fast(T v) {
  return norm2pi_fast(v + std::numbers::pi_v<T>) - std::numbers::pi_v<T>;
}

} // namespace emb
