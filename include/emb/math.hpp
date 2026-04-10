#pragma once

#include "../../src/math/trigonometric.hpp"

#include <algorithm>
#include <bit>
#include <cfloat>
#include <concepts>
#include <numbers>

#ifdef __arm__
extern "C" {
#include "arm_math.h"
}
#endif

#ifdef __x86_64__
#include <cmath>
#endif

namespace emb {

// sin -------------------------------------------------------------------------
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

// cos -------------------------------------------------------------------------
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

// atan2 -----------------------------------------------------------------------
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

// rsqrt/sqrt -------------------------------------------------------------
constexpr float fast_rsqrt(float arg) {
  assert(arg >= FLT_MIN);

  const float x2 = arg * 0.5f;

  auto i = std::bit_cast<uint32_t>(arg);
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

// fmod ------------------------------------------------------------------------
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

// sgn -------------------------------------------------------------------------
template<typename T = int, typename V>
constexpr T sgn(V v) {
  return T((V(0) < v) - (v < V(0)));
}

// iseven ----------------------------------------------------------------------
constexpr bool iseven(std::integral auto v) {
  return v % 2 == 0;
}

// isodd -----------------------------------------------------------------------
constexpr bool isodd(std::integral auto v) {
  return !iseven(v);
}

template<std::floating_point T>
constexpr T to_rad(T deg) {
  return std::numbers::pi_v<T> * deg / T{180};
}

template<std::floating_point T>
constexpr T to_deg(T rad) {
  return T{180} * rad / std::numbers::pi_v<T>;
}

template<std::floating_point T, std::integral P>
constexpr T to_eradps(T n, P p) {
  return 2 * std::numbers::pi_v<T> * static_cast<T>(p) * n / 60;
}

template<std::floating_point T, std::integral P>
constexpr T to_rpm(T w, P p) {
  return 60 * w / (2 * std::numbers::pi_v<T> * static_cast<T>(p));
}

template<std::floating_point T>
constexpr T rem2pi(T v) {
  constexpr T two_pi = 2 * std::numbers::pi_v<T>;
  v = emb::fmod(v, two_pi);
  if (v < 0) {
    v += two_pi;
  }
  return v;
}

template<std::floating_point T>
constexpr T rempi(T v) {
  return rem2pi(v + std::numbers::pi_v<T>) - std::numbers::pi_v<T>;
}

template<std::floating_point T>
constexpr T rem2pi_fast(T v) {
  constexpr T two_pi = 2 * std::numbers::pi_v<T>;
  constexpr T inv_two_pi = 1 / (2 * std::numbers::pi_v<T>);

  T norm = v * inv_two_pi;
  norm -= static_cast<T>(static_cast<int32_t>(norm) - (norm < T{0}));
  if (norm >= T{1}) norm -= T{1};
  return norm * two_pi;
}

template<std::floating_point T>
constexpr T rempi_fast(T v) {
  return rem2pi_fast(v + std::numbers::pi_v<T>) - std::numbers::pi_v<T>;
}

template<typename T>
class range {
private:
  T lower_;
  T upper_;
public:
  range(T const& v1, T const& v2) {
    if (v1 < v2) {
      lower_ = v1;
      upper_ = v2;
    } else {
      lower_ = v2;
      upper_ = v1;
    }
  }

  bool contains(T const& v) const {
    return (lower_ <= v) && (v <= upper_);
  }

  T const& lower_bound() const {
    return lower_;
  }

  void set_lower_bound(T const& v) {
    if (v <= upper_) {
      lower_ = v;
    }
  }

  T const& upper_bound() const {
    return upper_;
  }

  void set_upper_bound(T const& v) {
    if (v >= lower_) {
      upper_ = v;
    }
  }

  T length() const {
    return upper_ - lower_;
  }
};

template<typename T, typename Time>
class integrator {
private:
  T sum_;
  Time ts_;
  T initval_;
public:
  range<T> output_range;

  integrator(Time const& ts, range<T> const& outrange, T const& initval)
      : ts_(ts), initval_(initval), output_range(outrange) {
    reset();
  }

  void push(T const& v) {
    sum_ = std::clamp(
        sum_ + v * ts_,
        output_range.lower_bound(),
        output_range.upper_bound()
    );
  }

  void add(T const& v) {
    sum_ = std::clamp(
        sum_ + v,
        output_range.lower_bound(),
        output_range.upper_bound()
    );
  }

  T const& output() const {
    return sum_;
  }

  void reset() {
    sum_ = std::clamp(
        initval_,
        output_range.lower_bound(),
        output_range.upper_bound()
    );
  }

  void set_timestep(float ts) {
    ts_ = ts;
  }
};

class signed_pu {
private:
  float v_;
public:
  constexpr signed_pu() : v_(0.0f) {}

  constexpr explicit signed_pu(float v) : v_(std::clamp(v, -1.0f, 1.0f)) {}

  constexpr signed_pu(float v, float base)
      : v_(std::clamp(v / base, -1.0f, 1.0f)) {}

  constexpr float value() const {
    return v_;
  }

  constexpr signed_pu& operator+=(signed_pu const& rhs) {
    set(v_ + rhs.v_);
    return *this;
  }

  constexpr signed_pu& operator-=(signed_pu const& rhs) {
    set(v_ - rhs.v_);
    return *this;
  }
private:
  constexpr void set(float v) {
    v_ = std::clamp(v, -1.0f, 1.0f);
  }
};

constexpr bool operator==(signed_pu const& lhs, signed_pu const& rhs) {
  return lhs.value() == rhs.value();
}

constexpr bool operator!=(signed_pu const& lhs, signed_pu const& rhs) {
  return lhs.value() != rhs.value();
}

constexpr bool operator<(signed_pu const& lhs, signed_pu const& rhs) {
  return lhs.value() < rhs.value();
}

constexpr bool operator>(signed_pu const& lhs, signed_pu const& rhs) {
  return lhs.value() > rhs.value();
}

constexpr bool operator<=(signed_pu const& lhs, signed_pu const& rhs) {
  return lhs.value() <= rhs.value();
}

constexpr bool operator>=(signed_pu const& lhs, signed_pu const& rhs) {
  return lhs.value() >= rhs.value();
}

constexpr signed_pu operator+(signed_pu const& lhs, signed_pu const& rhs) {
  signed_pu tmp = lhs;
  return tmp += rhs;
}

constexpr signed_pu operator-(signed_pu const& lhs, signed_pu const& rhs) {
  signed_pu tmp = lhs;
  return tmp -= rhs;
}

constexpr signed_pu operator*(signed_pu const& lhs, float rhs) {
  return signed_pu(lhs.value() * rhs);
}

constexpr signed_pu operator*(float lhs, signed_pu const& rhs) {
  return rhs * lhs;
}

constexpr signed_pu operator/(signed_pu const& lhs, float rhs) {
  return signed_pu(lhs.value() / rhs);
}

class unsigned_pu {
private:
  float v_;
public:
  constexpr unsigned_pu() : v_(0.0f) {}

  constexpr explicit unsigned_pu(float v) : v_(std::clamp(v, 0.0f, 1.0f)) {}

  constexpr unsigned_pu(float v, float base)
      : v_(std::clamp(v / base, 0.0f, 1.0f)) {}

  constexpr float value() const {
    return v_;
  }

  constexpr unsigned_pu& operator+=(unsigned_pu const& rhs) {
    set(v_ + rhs.v_);
    return *this;
  }

  constexpr unsigned_pu& operator-=(unsigned_pu const& rhs) {
    set(v_ - rhs.v_);
    return *this;
  }
private:
  constexpr void set(float v) {
    v_ = std::clamp(v, 0.0f, 1.0f);
  }
};

constexpr bool operator==(unsigned_pu const& lhs, unsigned_pu const& rhs) {
  return lhs.value() == rhs.value();
}

constexpr bool operator!=(unsigned_pu const& lhs, unsigned_pu const& rhs) {
  return lhs.value() != rhs.value();
}

constexpr bool operator<(unsigned_pu const& lhs, unsigned_pu const& rhs) {
  return lhs.value() < rhs.value();
}

constexpr bool operator>(unsigned_pu const& lhs, unsigned_pu const& rhs) {
  return lhs.value() > rhs.value();
}

constexpr bool operator<=(unsigned_pu const& lhs, unsigned_pu const& rhs) {
  return lhs.value() <= rhs.value();
}

constexpr bool operator>=(unsigned_pu const& lhs, unsigned_pu const& rhs) {
  return lhs.value() >= rhs.value();
}

constexpr unsigned_pu
operator+(unsigned_pu const& lhs, unsigned_pu const& rhs) {
  unsigned_pu tmp = lhs;
  return tmp += rhs;
}

constexpr unsigned_pu
operator-(unsigned_pu const& lhs, unsigned_pu const& rhs) {
  unsigned_pu tmp = lhs;
  return tmp -= rhs;
}

constexpr unsigned_pu operator*(unsigned_pu const& lhs, float rhs) {
  return unsigned_pu(lhs.value() * rhs);
}

constexpr unsigned_pu operator*(float lhs, unsigned_pu const& rhs) {
  return rhs * lhs;
}

constexpr unsigned_pu operator/(unsigned_pu const& lhs, float rhs) {
  return unsigned_pu(lhs.value() / rhs);
}

} // namespace emb
