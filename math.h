#pragma once

#include <emblib/algorithm.h>
#include <emblib/core.h>

#if defined(EMBLIB_C28X)
#include <float.h>
#include <math.h>
#include <motorcontrol/math.h>
#elif defined(EMBLIB_ARM)
#include <algorithm>
extern "C" {
#include "arm_math.h"
}
#endif

#include <limits.h>

namespace emb {

namespace numbers {

#if defined(EMBLIB_C28X)

const float pi = MATH_PI;
const float pi_over_2 = MATH_PI_OVER_TWO;
const float pi_over_4 = MATH_PI_OVER_FOUR;
const float pi_over_3 = MATH_PI / 3;
const float pi_over_6 = MATH_PI / 6;
const float two_pi = MATH_TWO_PI;

const float sqrt_2 = sqrtf(2.f);
const float sqrt_3 = sqrtf(3.f);

const float inv_sqrt3 = 0.57735026918963f;

#elif defined(EMBLIB_ARM)

inline constexpr float pi = PI;
inline constexpr float pi_over_2 = pi / 2;
inline constexpr float pi_over_4 = pi / 4;
inline constexpr float pi_over_3 = pi / 3;
inline constexpr float pi_over_6 = pi / 6;
inline constexpr float two_pi = 2 * pi;

inline float sqrt_2 = std::sqrt(2.f);
inline float sqrt_3 = std::sqrt(3.f);

inline constexpr float inv_sqrt3 = 0.57735026918963f;

#endif

} // namespace numbers

#if defined(EMBLIB_C28X)

template<typename T>
inline int sgn(T v) { return (v > T(0)) - (v < T(0)); }

inline float to_rad(float deg) { return numbers::pi * deg / 180; }

inline float to_deg(float rad) { return 180 * rad / numbers::pi; }

#elif defined(EMBLIB_ARM)

template<typename T>
constexpr int sgn(T v) { return (v > T(0)) - (v < T(0)); }

constexpr float to_rad(float deg) { return numbers::pi * deg / 180; }

constexpr float to_deg(float rad) { return 180 * rad / numbers::pi; }

constexpr bool ispowerof2(unsigned int v) { return v && ((v & (v - 1)) == 0); }

#endif

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
    range(const T& v1, const T& v2) {
        if (v1 < v2) {
            _lower = v1;
            _upper = v2;
        } else {
            _lower = v2;
            _upper = v1;
        }
    }

    bool contains(const T& v) const { return (_lower <= v) && (v <= _upper); }

    const T& lower_bound() const { return _lower; }
    void set_lower_bound(const T& v) {
        if (v <= _upper) {
            _lower = v;
        }
    }

    const T& upper_bound() const { return _upper; }
    void set_upper_bound(const T& v) {
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

    integrator(const Time& ts_,
               const range<T>& output_range_,
               const T& initvalue_)
            : _ts(ts_), _initval(initvalue_), output_range(output_range_) {
        reset();
    }

    void push(const T& v) {
        _sum = clamp(_sum + v * _ts,
                     output_range.lower_bound(),
                     output_range.upper_bound());
    }

    void add(const T& v) {
        _sum = clamp(_sum + v,
                     output_range.lower_bound(),
                     output_range.upper_bound());
    }

    const T& output() const { return _sum; }
    void reset() {
        _sum = clamp(_initval,
                     output_range.lower_bound(),
                     output_range.upper_bound());
    }

    void set_sampling_period(float v) { _ts = v; }
};

class signed_perunit {
private:
    float _value;
public:
    signed_perunit() : _value(0.f) {}
    explicit signed_perunit(float v) : _value(emb::clamp(v, -1.f, 1.f)) {}
    signed_perunit(float v, float base)
            : _value(emb::clamp(v / base, -1.f, 1.f)) {}

    float get() const { return _value; }
    void set(float v) { _value = emb::clamp(v, -1.f, 1.f); }

    signed_perunit& operator+=(const signed_perunit& rhs) {
        set(_value + rhs._value);
        return *this;
    }

    signed_perunit& operator-=(const signed_perunit& rhs) {
        set(_value - rhs._value);
        return *this;
    }
};

inline bool operator<(const signed_perunit& lhs, const signed_perunit& rhs) {
    return lhs.get() < rhs.get();
}

inline bool operator>(const signed_perunit& lhs, const signed_perunit& rhs) {
    return rhs < lhs;
}

inline bool operator<=(const signed_perunit& lhs, const signed_perunit& rhs) {
    return !(lhs > rhs);
}

inline bool operator>=(const signed_perunit& lhs, const signed_perunit& rhs) {
    return !(lhs < rhs);
}

inline signed_perunit operator+(const signed_perunit& lhs,
                                const signed_perunit& rhs) {
    signed_perunit tmp = lhs;
    return tmp += rhs;
}

inline signed_perunit operator-(const signed_perunit& lhs,
                                const signed_perunit& rhs) {
    signed_perunit tmp = lhs;
    return tmp -= rhs;
}

inline signed_perunit operator*(const signed_perunit& lhs, float rhs) {
    return signed_perunit(lhs.get() * rhs);
}

inline signed_perunit operator*(float lhs, const signed_perunit& rhs) {
    return rhs * lhs;
}

inline signed_perunit operator/(const signed_perunit& lhs, float rhs) {
    return signed_perunit(lhs.get() / rhs);
}

class unsigned_perunit {
private:
    float _value;
public:
    unsigned_perunit() : _value(0.f) {}
    explicit unsigned_perunit(float v) : _value(emb::clamp(v, 0.f, 1.f)) {}
    unsigned_perunit(float v, float base)
            : _value(emb::clamp(v / base, 0.f, 1.f)) {}

    float get() const { return _value; }
    void set(float v) { _value = emb::clamp(v, 0.f, 1.f); }

    unsigned_perunit& operator+=(const unsigned_perunit& rhs) {
        set(_value + rhs._value);
        return *this;
    }

    unsigned_perunit& operator-=(const unsigned_perunit& rhs) {
        set(_value - rhs._value);
        return *this;
    }
};

inline bool operator<(const unsigned_perunit& lhs,
                      const unsigned_perunit& rhs) {
    return lhs.get() < rhs.get();
}

inline bool operator>(const unsigned_perunit& lhs,
                      const unsigned_perunit& rhs) {
    return rhs < lhs;
}

inline bool operator<=(const unsigned_perunit& lhs,
                       const unsigned_perunit& rhs) {
    return !(lhs > rhs);
}

inline bool operator>=(const unsigned_perunit& lhs,
                       const unsigned_perunit& rhs) {
    return !(lhs < rhs);
}

inline unsigned_perunit operator+(const unsigned_perunit& lhs,
                                  const unsigned_perunit& rhs) {
    unsigned_perunit tmp = lhs;
    return tmp += rhs;
}

inline unsigned_perunit operator-(const unsigned_perunit& lhs,
                                  const unsigned_perunit& rhs) {
    unsigned_perunit tmp = lhs;
    return tmp -= rhs;
}

inline unsigned_perunit operator*(const unsigned_perunit& lhs, float rhs) {
    return unsigned_perunit(lhs.get() * rhs);
}

inline unsigned_perunit operator*(float lhs, const unsigned_perunit& rhs) {
    return rhs * lhs;
}

inline unsigned_perunit operator/(const unsigned_perunit& lhs, float rhs) {
    return unsigned_perunit(lhs.get() / rhs);
}

} // namespace emb
