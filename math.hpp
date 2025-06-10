#pragma once

#include <emblib/algorithm.hpp>
#include <emblib/core.hpp>

#if defined(EMBLIB_C28X)
#include <float.h>
#include <math.h>
#include <motorcontrol/math.h>
#elif defined(EMBLIB_ARM)
extern "C" {
#include "arm_math.h"
}
#endif

#include <limits.h>

namespace emb {

namespace numbers {
#ifdef __cpp_inline_variables
inline constexpr float pi = PI;
inline constexpr float pi_over_2 = pi / 2;
inline constexpr float pi_over_4 = pi / 4;
inline constexpr float pi_over_3 = pi / 3;
inline constexpr float pi_over_6 = pi / 6;
inline constexpr float two_pi = 2 * pi;
inline float sqrt_2 = std::sqrt(2.f);
inline float sqrt_3 = std::sqrt(3.f);
inline constexpr float inv_sqrt3 = 0.57735026918963f;
#else
const float pi = MATH_PI;
float const pi_over_2 = MATH_PI_OVER_TWO;
float const pi_over_4 = MATH_PI_OVER_FOUR;
float const pi_over_3 = MATH_PI / 3;
float const pi_over_6 = MATH_PI / 6;
float const two_pi = MATH_TWO_PI;
float const sqrt_2 = sqrtf(2.f);
float const sqrt_3 = sqrtf(3.f);
float const inv_sqrt3 = 0.57735026918963f;
#endif
} // namespace numbers

template<typename T>
EMB_CONSTEXPR int sgn(T v) {
  return (v > T(0)) - (v < T(0));
}

EMB_CONSTEXPR float to_rad(float deg) {
  return numbers::pi * deg / 180.0f;
}

EMB_CONSTEXPR float to_deg(float rad) {
  return 180.0f * rad / numbers::pi;
}

EMB_CONSTEXPR float to_eradps(float n, int p) {
  return numbers::two_pi * float(p) * n / 60.f;
}

EMB_CONSTEXPR float to_rpm(float w, int p) {
  return 60.f * w / (numbers::two_pi * float(p));
}

EMB_CONSTEXPR bool ispow2(unsigned int v) {
  return v && ((v & (v - 1)) == 0);
}

inline float rem_2pi(float v) {
  v = fmodf(v, numbers::two_pi);
  if (v < 0) {
    v += numbers::two_pi;
  }
  return v;
}

inline float rem_pi(float v) {
  v = fmodf(v + numbers::pi, numbers::two_pi);
  if (v < 0) {
    v += numbers::two_pi;
  }
  return v - numbers::pi;
}

template<typename T>
class range {
private:
  T _lower;
  T _upper;
public:
  range(T const& v1, T const& v2) {
    if (v1 < v2) {
      _lower = v1;
      _upper = v2;
    } else {
      _lower = v2;
      _upper = v1;
    }
  }

  bool contains(T const& v) const { return (_lower <= v) && (v <= _upper); }

  T const& lower_bound() const { return _lower; }

  void set_lower_bound(T const& v) {
    if (v <= _upper) {
      _lower = v;
    }
  }

  T const& upper_bound() const { return _upper; }

  void set_upper_bound(T const& v) {
    if (v >= _lower) {
      _upper = v;
    }
  }

  T length() const { return _upper - _lower; }
};

template<typename T, typename Time>
class integrator {
private:
  T _sum;
  Time _ts;
  T _initval;
public:
  range<T> output_range;

  integrator(Time const& ts_,
             range<T> const& output_range_,
             T const& initvalue_)
      : _ts(ts_), _initval(initvalue_), output_range(output_range_) {
    reset();
  }

  void push(T const& v) {
    _sum = clamp(
        _sum + v * _ts, output_range.lower_bound(), output_range.upper_bound());
  }

  void add(T const& v) {
    _sum =
        clamp(_sum + v, output_range.lower_bound(), output_range.upper_bound());
  }

  T const& output() const { return _sum; }

  void reset() {
    _sum =
        clamp(_initval, output_range.lower_bound(), output_range.upper_bound());
  }

  void set_sampling_period(float v) { _ts = v; }
};

class signed_pu {
private:
  float v_;
public:
  signed_pu() : v_(0.f) {}

  explicit signed_pu(float v) : v_(emb::clamp(v, -1.f, 1.f)) {}

  signed_pu(float v, float base) : v_(emb::clamp(v / base, -1.f, 1.f)) {}

  float numval() const { return v_; }

  signed_pu& operator+=(signed_pu const& rhs) {
    set(v_ + rhs.v_);
    return *this;
  }

  signed_pu& operator-=(signed_pu const& rhs) {
    set(v_ - rhs.v_);
    return *this;
  }
private:
  void set(float v) { v_ = emb::clamp(v, -1.f, 1.f); }
};

inline bool operator==(signed_pu const& lhs, signed_pu const& rhs) {
  return lhs.numval() == rhs.numval();
}

inline bool operator!=(signed_pu const& lhs, signed_pu const& rhs) {
  return lhs.numval() != rhs.numval();
}

inline bool operator<(signed_pu const& lhs, signed_pu const& rhs) {
  return lhs.numval() < rhs.numval();
}

inline bool operator>(signed_pu const& lhs, signed_pu const& rhs) {
  return lhs.numval() > rhs.numval();
}

inline bool operator<=(signed_pu const& lhs, signed_pu const& rhs) {
  return lhs.numval() <= rhs.numval();
}

inline bool operator>=(signed_pu const& lhs, signed_pu const& rhs) {
  return lhs.numval() >= rhs.numval();
}

inline signed_pu operator+(signed_pu const& lhs, signed_pu const& rhs) {
  signed_pu tmp = lhs;
  return tmp += rhs;
}

inline signed_pu operator-(signed_pu const& lhs, signed_pu const& rhs) {
  signed_pu tmp = lhs;
  return tmp -= rhs;
}

inline signed_pu operator*(signed_pu const& lhs, float rhs) {
  return signed_pu(lhs.numval() * rhs);
}

inline signed_pu operator*(float lhs, signed_pu const& rhs) {
  return rhs * lhs;
}

inline signed_pu operator/(signed_pu const& lhs, float rhs) {
  return signed_pu(lhs.numval() / rhs);
}

class unsigned_pu {
private:
  float v_;
public:
  unsigned_pu() : v_(0.f) {}

  explicit unsigned_pu(float v) : v_(emb::clamp(v, 0.f, 1.f)) {}

  unsigned_pu(float v, float base) : v_(emb::clamp(v / base, 0.f, 1.f)) {}

  float numval() const { return v_; }

  unsigned_pu& operator+=(unsigned_pu const& rhs) {
    set(v_ + rhs.v_);
    return *this;
  }

  unsigned_pu& operator-=(unsigned_pu const& rhs) {
    set(v_ - rhs.v_);
    return *this;
  }
private:
  void set(float v) { v_ = emb::clamp(v, 0.f, 1.f); }
};

inline bool operator==(unsigned_pu const& lhs, unsigned_pu const& rhs) {
  return lhs.numval() == rhs.numval();
}

inline bool operator!=(unsigned_pu const& lhs, unsigned_pu const& rhs) {
  return lhs.numval() != rhs.numval();
}

inline bool operator<(unsigned_pu const& lhs, unsigned_pu const& rhs) {
  return lhs.numval() < rhs.numval();
}

inline bool operator>(unsigned_pu const& lhs, unsigned_pu const& rhs) {
  return lhs.numval() > rhs.numval();
}

inline bool operator<=(unsigned_pu const& lhs, unsigned_pu const& rhs) {
  return lhs.numval() <= rhs.numval();
}

inline bool operator>=(unsigned_pu const& lhs, unsigned_pu const& rhs) {
  return lhs.numval() >= rhs.numval();
}

inline unsigned_pu operator+(unsigned_pu const& lhs, unsigned_pu const& rhs) {
  unsigned_pu tmp = lhs;
  return tmp += rhs;
}

inline unsigned_pu operator-(unsigned_pu const& lhs, unsigned_pu const& rhs) {
  unsigned_pu tmp = lhs;
  return tmp -= rhs;
}

inline unsigned_pu operator*(unsigned_pu const& lhs, float rhs) {
  return unsigned_pu(lhs.numval() * rhs);
}

inline unsigned_pu operator*(float lhs, unsigned_pu const& rhs) {
  return rhs * lhs;
}

inline unsigned_pu operator/(unsigned_pu const& lhs, float rhs) {
  return unsigned_pu(lhs.numval() / rhs);
}

} // namespace emb
