#pragma once

#include "../../src/math/trigonometric.hpp"
#include <emb/algorithm.hpp>
#include <emb/core.hpp>
#include <emb/numbers.hpp>

#ifdef __c28x__
#include <math.h>
#endif

#ifdef __arm__
extern "C" {
#include "arm_math.h"
}
#endif

#ifdef __x86_64__
#include <cmath>
#endif

namespace emb {

inline float builtin_sinf(float x) {
#ifdef __c28x__
  return sinf(x);
#endif
#ifdef __arm__
  return arm_sin_f32(x);
#endif
#ifdef __x86_64__
  return std::sinf(x);
#endif
}

EMB_INLINE_CONSTEXPR float sin(float x) {
#ifdef __cpp_if_consteval
  if !consteval {
    return builtin_sinf(x);
  } else {
    return lookup_sinf(x);
  }
#else
  return builtin_sinf(x);
#endif
}

#ifdef __cpp_consteval
consteval float fmod_trivial(float x, float y) {
  return x - static_cast<float>(static_cast<long long>(x / y)) * y;
}
#endif

EMB_INLINE_CONSTEXPR float fmod(float x, float y) {
#ifdef __cpp_if_consteval
  if !consteval {
    return std::fmod(x, y);
  } else {
    return fmod_trivial(x, y);
  }
#else
  return std::fmod(x, y);
#endif
}

template<typename T = int, typename V>
EMB_INLINE_CONSTEXPR T sgn(V v) {
  return T((V(0) < v) - (v < V(0)));
}

#if __cplusplus < 202000

EMB_INLINE_CONSTEXPR bool ispow2(unsigned int v) {
  return v && ((v & (v - 1)) == 0);
}

#endif

#ifdef __cpp_concepts

constexpr bool iseven(std::integral auto v) {
  return v % 2 == 0;
}

constexpr bool isodd(std::integral auto v) {
  return !iseven(v);
}

#endif

EMB_INLINE_CONSTEXPR float to_rad(float deg) {
  return numbers::pi * deg / 180.0f;
}

EMB_INLINE_CONSTEXPR float to_deg(float rad) {
  return 180.0f * rad / numbers::pi;
}

EMB_INLINE_CONSTEXPR float to_eradps(float n, int32_t p) {
  return 2 * numbers::pi * float(p) * n / 60.0f;
}

EMB_INLINE_CONSTEXPR float to_rpm(float w, int32_t p) {
  return 60.f * w / (2 * numbers::pi * float(p));
}

EMB_INLINE_CONSTEXPR float rem2pi(float v) {
  v = emb::fmod(v, 2 * numbers::pi);
  if (v < 0) {
    v += 2 * numbers::pi;
  }
  return v;
}

EMB_INLINE_CONSTEXPR float rempi(float v) {
  v = emb::fmod(v + numbers::pi, 2 * numbers::pi);
  if (v < 0) {
    v += 2 * numbers::pi;
  }
  return v - numbers::pi;
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
    sum_ = clamp(
        sum_ + v * ts_,
        output_range.lower_bound(),
        output_range.upper_bound()
    );
  }

  void add(T const& v) {
    sum_ = clamp(
        sum_ + v,
        output_range.lower_bound(),
        output_range.upper_bound()
    );
  }

  T const& output() const {
    return sum_;
  }

  void reset() {
    sum_ = clamp(
        initval_,
        output_range.lower_bound(),
        output_range.upper_bound()
    );
  }

  void set_sampling_period(float ts) {
    ts_ = ts;
  }
};

class signed_pu {
private:
  float v_;
public:
  EMB_CONSTEXPR signed_pu() : v_(0.0f) {}

  EMB_CONSTEXPR explicit signed_pu(float v) : v_(emb::clamp(v, -1.0f, 1.0f)) {}

  EMB_CONSTEXPR signed_pu(float v, float base)
      : v_(emb::clamp(v / base, -1.0f, 1.0f)) {}

  EMB_CONSTEXPR float numval() const {
    return v_;
  }

  EMB_CONSTEXPR signed_pu& operator+=(signed_pu const& rhs) {
    set(v_ + rhs.v_);
    return *this;
  }

  EMB_CONSTEXPR signed_pu& operator-=(signed_pu const& rhs) {
    set(v_ - rhs.v_);
    return *this;
  }
private:
  EMB_CONSTEXPR void set(float v) {
    v_ = emb::clamp(v, -1.0f, 1.0f);
  }
};

EMB_INLINE_CONSTEXPR bool operator==(
    signed_pu const& lhs,
    signed_pu const& rhs
) {
  return lhs.numval() == rhs.numval();
}

EMB_INLINE_CONSTEXPR bool operator!=(
    signed_pu const& lhs,
    signed_pu const& rhs
) {
  return lhs.numval() != rhs.numval();
}

EMB_INLINE_CONSTEXPR bool operator<(
    signed_pu const& lhs,
    signed_pu const& rhs
) {
  return lhs.numval() < rhs.numval();
}

EMB_INLINE_CONSTEXPR bool operator>(
    signed_pu const& lhs,
    signed_pu const& rhs
) {
  return lhs.numval() > rhs.numval();
}

EMB_INLINE_CONSTEXPR bool operator<=(
    signed_pu const& lhs,
    signed_pu const& rhs
) {
  return lhs.numval() <= rhs.numval();
}

EMB_INLINE_CONSTEXPR bool operator>=(
    signed_pu const& lhs,
    signed_pu const& rhs
) {
  return lhs.numval() >= rhs.numval();
}

EMB_INLINE_CONSTEXPR signed_pu
operator+(signed_pu const& lhs, signed_pu const& rhs) {
  signed_pu tmp = lhs;
  return tmp += rhs;
}

EMB_INLINE_CONSTEXPR signed_pu
operator-(signed_pu const& lhs, signed_pu const& rhs) {
  signed_pu tmp = lhs;
  return tmp -= rhs;
}

EMB_INLINE_CONSTEXPR signed_pu operator*(signed_pu const& lhs, float rhs) {
  return signed_pu(lhs.numval() * rhs);
}

EMB_INLINE_CONSTEXPR signed_pu operator*(float lhs, signed_pu const& rhs) {
  return rhs * lhs;
}

EMB_INLINE_CONSTEXPR signed_pu operator/(signed_pu const& lhs, float rhs) {
  return signed_pu(lhs.numval() / rhs);
}

class unsigned_pu {
private:
  float v_;
public:
  EMB_CONSTEXPR unsigned_pu() : v_(0.0f) {}

  EMB_CONSTEXPR explicit unsigned_pu(float v) : v_(emb::clamp(v, 0.0f, 1.0f)) {}

  EMB_CONSTEXPR unsigned_pu(float v, float base)
      : v_(emb::clamp(v / base, 0.0f, 1.0f)) {}

  EMB_CONSTEXPR float numval() const {
    return v_;
  }

  EMB_CONSTEXPR unsigned_pu& operator+=(unsigned_pu const& rhs) {
    set(v_ + rhs.v_);
    return *this;
  }

  EMB_CONSTEXPR unsigned_pu& operator-=(unsigned_pu const& rhs) {
    set(v_ - rhs.v_);
    return *this;
  }
private:
  EMB_CONSTEXPR void set(float v) {
    v_ = emb::clamp(v, 0.0f, 1.0f);
  }
};

EMB_INLINE_CONSTEXPR bool operator==(
    unsigned_pu const& lhs,
    unsigned_pu const& rhs
) {
  return lhs.numval() == rhs.numval();
}

EMB_INLINE_CONSTEXPR bool operator!=(
    unsigned_pu const& lhs,
    unsigned_pu const& rhs
) {
  return lhs.numval() != rhs.numval();
}

EMB_INLINE_CONSTEXPR bool operator<(
    unsigned_pu const& lhs,
    unsigned_pu const& rhs
) {
  return lhs.numval() < rhs.numval();
}

EMB_INLINE_CONSTEXPR bool operator>(
    unsigned_pu const& lhs,
    unsigned_pu const& rhs
) {
  return lhs.numval() > rhs.numval();
}

EMB_INLINE_CONSTEXPR bool operator<=(
    unsigned_pu const& lhs,
    unsigned_pu const& rhs
) {
  return lhs.numval() <= rhs.numval();
}

EMB_INLINE_CONSTEXPR bool operator>=(
    unsigned_pu const& lhs,
    unsigned_pu const& rhs
) {
  return lhs.numval() >= rhs.numval();
}

EMB_INLINE_CONSTEXPR unsigned_pu
operator+(unsigned_pu const& lhs, unsigned_pu const& rhs) {
  unsigned_pu tmp = lhs;
  return tmp += rhs;
}

EMB_INLINE_CONSTEXPR unsigned_pu
operator-(unsigned_pu const& lhs, unsigned_pu const& rhs) {
  unsigned_pu tmp = lhs;
  return tmp -= rhs;
}

EMB_INLINE_CONSTEXPR unsigned_pu operator*(unsigned_pu const& lhs, float rhs) {
  return unsigned_pu(lhs.numval() * rhs);
}

EMB_INLINE_CONSTEXPR unsigned_pu operator*(float lhs, unsigned_pu const& rhs) {
  return rhs * lhs;
}

EMB_INLINE_CONSTEXPR unsigned_pu operator/(unsigned_pu const& lhs, float rhs) {
  return unsigned_pu(lhs.numval() / rhs);
}

} // namespace emb
