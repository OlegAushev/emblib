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
inline constexpr float pi_over_2 = pi / 2.0f;
inline constexpr float pi_over_4 = pi / 4.0f;
inline constexpr float pi_over_3 = pi / 3.0f;
inline constexpr float pi_over_6 = pi / 6.0f;
inline constexpr float two_pi = 2.0f * pi;
inline float sqrt_2 = std::sqrt(2.0f);
inline float sqrt_3 = std::sqrt(3.0f);
inline constexpr float inv_sqrt3 = 0.57735026918963f;
#else
const float pi = MATH_PI;
float const pi_over_2 = MATH_PI_OVER_TWO;
float const pi_over_4 = MATH_PI_OVER_FOUR;
float const pi_over_3 = MATH_PI / 3.0f;
float const pi_over_6 = MATH_PI / 6.0f;
float const two_pi = MATH_TWO_PI;
float const sqrt_2 = sqrtf(2.0f);
float const sqrt_3 = sqrtf(3.0f);
float const inv_sqrt3 = 0.57735026918963f;
#endif
} // namespace numbers

template<typename T>
EMB_CONSTEXPR int sgn(T v) {
  return (T(0) < v) - (v < T(0));
}

EMB_CONSTEXPR float to_rad(float deg) {
  return numbers::pi * deg / 180.0f;
}

EMB_CONSTEXPR float to_deg(float rad) {
  return 180.0f * rad / numbers::pi;
}

EMB_CONSTEXPR float to_eradps(float n, int p) {
  return numbers::two_pi * float(p) * n / 60.0f;
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

  bool contains(T const& v) const { return (lower_ <= v) && (v <= upper_); }

  T const& lower_bound() const { return lower_; }

  void set_lower_bound(T const& v) {
    if (v <= upper_) {
      lower_ = v;
    }
  }

  T const& upper_bound() const { return upper_; }

  void set_upper_bound(T const& v) {
    if (v >= lower_) {
      upper_ = v;
    }
  }

  T length() const { return upper_ - lower_; }
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
        sum_ + v * ts_, output_range.lower_bound(), output_range.upper_bound());
  }

  void add(T const& v) {
    sum_ =
        clamp(sum_ + v, output_range.lower_bound(), output_range.upper_bound());
  }

  T const& output() const { return sum_; }

  void reset() {
    sum_ =
        clamp(initval_, output_range.lower_bound(), output_range.upper_bound());
  }

  void set_sampling_period(float ts) { ts_ = ts; }
};

class signed_pu {
private:
  float v_;
public:
  signed_pu() : v_(0.0f) {}

  explicit signed_pu(float v) : v_(emb::clamp(v, -1.0f, 1.0f)) {}

  signed_pu(float v, float base) : v_(emb::clamp(v / base, -1.0f, 1.0f)) {}

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
  void set(float v) { v_ = emb::clamp(v, -1.0f, 1.0f); }
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
  unsigned_pu() : v_(0.0f) {}

  explicit unsigned_pu(float v) : v_(emb::clamp(v, 0.0f, 1.0f)) {}

  unsigned_pu(float v, float base) : v_(emb::clamp(v / base, 0.0f, 1.0f)) {}

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
  void set(float v) { v_ = emb::clamp(v, 0.0f, 1.0f); }
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
