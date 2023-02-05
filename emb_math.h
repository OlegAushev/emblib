#pragma once


#include <c28x_emb/emb_algorithm.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include "motorcontrol/math.h"
#include "limits.h"
#include "float.h"



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


inline float normalize_2pi(float value)
{
	value = fmodf(value, numbers::two_pi);
	if (value < 0)
	{
		value += numbers::two_pi;
	}
	return value;
}


inline float normalize_pi(float value)
{
	value = fmodf(value + numbers::pi, numbers::two_pi);
	if (value < 0)
	{
		value += numbers::two_pi;
	}
	return value - numbers::pi;
}


template <typename T>
class Range
{
private:
	T _lo;
	T _hi;
public:
	Range(const T& val1, const T& val2)
	{
		if (val1 < val2)
		{
			_lo = val1;
			_hi = val2;
		}
		else
		{
			_lo = val2;
			_hi = val1;
		}
	}

	bool contains(const T& val) const { return (_lo <= val) && (val <= _hi); }

	const T& lo() const { return _lo; }
	void set_lo(const T& value)
	{
		if (value <= _hi)
		{
			_lo = value;
		}
	}

	const T& hi() const { return _hi; }
	void set_hi(const T& value)
	{
		if (value >= _lo)
		{
			_hi = value;
		}
	}
};


template <typename T, typename Time>
class Integrator
{
private:
	T _sum;
	Time _dt;
	T _init;
public:
	Range<T> range;

	Integrator(const Range<T>& range_, const Time& dt_, const T& init_)
		: range(range_)
		, _dt(dt_)
		, _init(init_)
	{
		reset();
	}

	void integrate(const T& value)
	{
		_sum = clamp(_sum + value * _dt, range.lo(), range.hi());
	}

	void add(const T& value)
	{
		_sum = clamp(_sum + value, range.lo(), range.hi());
	}

	const T& value() const { return _sum; }
	void reset()
	{
		_sum = clamp(_init, range.lo(), range.hi());
	}
};

} // namespace emb

