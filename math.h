#pragma once


#include <emblib_c28x/core.h>
#include <emblib_c28x/algorithm.h>
#include <motorcontrol/math.h>
#include <math.h>
#include <limits.h>
#include <float.h>



namespace emb {

namespace numbers {

const float pi = MATH_PI;
const float pi_over_2 = MATH_PI_OVER_TWO;
const float pi_over_4 = MATH_PI_OVER_FOUR;
const float pi_over_3 = MATH_PI / 3;
const float pi_over_6 = MATH_PI / 6;
const float two_pi = MATH_TWO_PI;

const float sqrt_2 = sqrtf(2.f);
const float sqrt_3 = sqrtf(3.f);

} // namespace numbers


template <typename T>
inline int sgn(T value) { return (value > T(0)) - (value < T(0)); }


inline float to_rad(float deg) { return numbers::pi * deg / 180; }


inline float to_deg(float rad) { return 180 * rad / numbers::pi; }


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
    range(const T& val1, const T& val2) {
        if (val1 < val2) {
            _lo = val1;
            _hi = val2;
        } else {
            _lo = val2;
            _hi = val1;
        }
    }

    bool contains(const T& val) const { return (_lo <= val) && (val <= _hi); }

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

    integrator(const Time& ts_, const range<T>& output_range_, const T& initval_)
            : _ts(ts_)
            , output_range(output_range_)
            , _initval(initval_) {
        reset();
    }

    void update(const T& value) {
        _sum = clamp(_sum + value * _ts, output_range.lo(), output_range.hi());
    }

    void add(const T& value) {
        _sum = clamp(_sum + value, output_range.lo(), output_range.hi());
    }

    const T& output() const { return _sum; }
    void reset() {
        _sum = clamp(_initval, output_range.lo(), output_range.hi());
    }
};

} // namespace emb

