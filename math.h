#pragma once


#include <emblib/core.h>
#include <emblib/algorithm.h>

#if defined(EMBLIB_C28X)
#include <motorcontrol/math.h>
#include <math.h>
#include <float.h>
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


template <typename T>
inline int sgn(T value) { return (value > T(0)) - (value < T(0)); }


inline float to_rad(float deg) { return numbers::pi * deg / 180; }


inline float to_deg(float rad) { return 180 * rad / numbers::pi; }


#elif defined(EMBLIB_ARM)


template <typename T>
constexpr int sgn(T value) { return (value > T(0)) - (value < T(0)); }


constexpr float to_rad(float deg) { return numbers::pi * deg / 180; }


constexpr float to_deg(float rad) { return 180 * rad / numbers::pi; }


#endif


inline float rem_2pi(float value) {
    value = fmodf(value, numbers::two_pi);
    if (value < 0) {
        value += numbers::two_pi;
    }
    return value;
}


inline float rem_pi(float value) {
    value = fmodf(value + numbers::pi, numbers::two_pi);
    if (value < 0) {
        value += numbers::two_pi;
    }
    return value - numbers::pi;
}


template <typename T>
class range {
private:
    T _lo;
    T _hi;
public:
    range(const T& value1, const T& value2) {
        if (value1 < value2) {
            _lo = value1;
            _hi = value2;
        } else {
            _lo = value2;
            _hi = value1;
        }
    }

    bool contains(const T& value) const { return (_lo <= value) && (value <= _hi); }

    const T& lo() const { return _lo; }
    void set_lo(const T& value) {
        if (value <= _hi) {
            _lo = value;
        }
    }

    const T& hi() const { return _hi; }
    void set_hi(const T& value) {
        if (value >= _lo) {
            _hi = value;
        }
    }
};


template <typename T, typename Time>
class integrator {
private:
    T _sum;
    Time _ts;
    T _initval;
public:
    range<T> output_range;

    integrator(const Time& ts_, const range<T>& output_range_, const T& initvalue_)
            : _ts(ts_)
            , _initval(initvalue_)
            , output_range(output_range_) {
        reset();
    }

    void push(const T& value) {
        _sum = clamp(_sum + value * _ts, output_range.lo(), output_range.hi());
    }

    void add(const T& value) {
        _sum = clamp(_sum + value, output_range.lo(), output_range.hi());
    }

    const T& output() const { return _sum; }
    void reset() {
        _sum = clamp(_initval, output_range.lo(), output_range.hi());
    }

    void set_sampling_period(float value) { _ts = value; }
};


} // namespace emb
