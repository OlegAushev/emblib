#pragma once

#include "../../src/math/trigonometric.hpp"

#include <algorithm>
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

inline float builtin_sinf(float x) {
#ifdef __arm__
  return arm_sin_f32(x);
#endif
#ifdef __x86_64__
  return std::sinf(x);
#endif
}

constexpr float sin(float x) {
  if !consteval {
    return builtin_sinf(x);
  } else {
    return lookup_sinf(x);
  }
}

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

template<typename T = int, typename V>
constexpr T sgn(V v) {
  return T((V(0) < v) - (v < V(0)));
}

constexpr bool iseven(std::integral auto v) {
  return v % 2 == 0;
}

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
  v = emb::fmod(v, 2 * std::numbers::pi_v<T>);
  if (v < 0) {
    v += 2 * std::numbers::pi_v<T>;
  }
  return v;
}

template<std::floating_point T>
constexpr T rempi(T v) {
  v = emb::fmod(v + std::numbers::pi_v<T>, 2 * std::numbers::pi_v<T>);
  if (v < 0) {
    v += 2 * std::numbers::pi_v<T>;
  }
  return v - std::numbers::pi_v<T>;
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

  void set_sampling_period(float ts) {
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
