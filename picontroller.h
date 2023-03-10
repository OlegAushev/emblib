#pragma once


#include <emblib_c28x/core.h>
#include <emblib_c28x/algorithm.h>
#include "float.h"


namespace emb {

SCOPED_ENUM_DECLARE_BEGIN(controller_logic)
{
	direct,
	inverse
}
SCOPED_ENUM_DECLARE_END(controller_logic)


template <controller_logic::enum_type Logic>
class abstract_pi_controller : public emb::noncopyable
{
protected:
	float _kp;		// proportional gain
	float _ki;		// integral gain
	float _dt;		// time slice
	float _integrator_sum;	// integrator sum;
	float _out_min;		// PI output minimum limit
	float _out_max;		// PI output maximum limit
	float _out;		// PI output;

	static float _error(float ref, float meas);
public:
	abstract_pi_controller(float kp, float ki, float dt, float out_min, float out_max)
		: _kp(kp)
		, _ki(ki)
		, _dt(dt)
		, _integrator_sum(0)
		, _out_min(out_min)
		, _out_max(out_max)
		, _out(0)
	{}

	virtual ~abstract_pi_controller() {}
	virtual void update(float ref, float meas) = 0;
	virtual void reset()
	{
		_integrator_sum = 0;
		_out = 0;
	}
	float output() const { return _out; }
	void set_output_min(float value) { _out_min = value; }
	void set_output_max(float value) { _out_max = value; }
	float output_min() const { return _out_min; }
	float output_max() const { return _out_max; }

	void set_kp(float value) { _kp = value; }
	void set_ki(float value) { _ki = value; }
	float kp() const { return _kp; }
	float ki() const { return _ki; }
	float sum() const { return _integrator_sum; }
};


inline float abstract_pi_controller<controller_logic::direct>::_error(float ref, float meas) { return ref - meas; }
inline float abstract_pi_controller<controller_logic::inverse>::_error(float ref, float meas) { return meas - ref; }


template <controller_logic::enum_type Logic>
class backcalc_pi_controller : public abstract_pi_controller<Logic>
{
protected:
	float _kc;	// anti-windup gain
public:
	backcalc_pi_controller(float kp, float ki, float dt, float kc, float out_min, float out_max)
		: abstract_pi_controller<Logic>(kp, ki, dt, out_min, out_max)
		, _kc(kc)
	{}

	virtual void update(float ref, float meas)
	{
		float error = abstract_pi_controller<Logic>::_error(ref, meas);
		float out = emb::clamp(error * this->_kp + this->_integrator_sum, -FLT_MAX, FLT_MAX);

		if (out > this->_out_max)
		{
			this->_out = this->_out_max;
		}
		else if (out < this->_out_min)
		{
			this->_out = this->_out_min;
		}
		else
		{
			this->_out = out;
		}

		this->_integrator_sum = emb::clamp(this->_integrator_sum + this->_ki * this->_dt * error - _kc * (out - this->_out),
				-FLT_MAX, FLT_MAX);
	}
};


template <controller_logic::enum_type Logic>
class clamping_pi_controller : public abstract_pi_controller<Logic>
{
protected:
	float _error;
public:
	clamping_pi_controller(float kp, float ki, float dt, float out_min, float out_max)
		: abstract_pi_controller<Logic>(kp, ki, dt, out_min, out_max)
		, _error(0)
	{}

	virtual void update(float ref, float meas)
	{
		float error = abstract_pi_controller<Logic>::_error(ref, meas);
		float outp = error * this->_kp;
		float sum_i = (error + _error) * 0.5f * this->_ki * this->_dt + this->_integrator_sum;
		_error = error;
		float out = outp + sum_i;

		if (out > this->_out_max)
		{
			this->_out = this->_out_max;
			if (outp < this->_out_max)
			{
				this->_integrator_sum = this->_out_max - outp;
			}
		}
		else if (out < this->_out_min)
		{
			this->_out = this->_out_min;
			if (outp > this->_out_min)
			{
				this->_integrator_sum = this->_out_min - outp;
			}
		}
		else
		{
			this->_out = out;
			this->_integrator_sum = sum_i;
		}
	}

	virtual void reset()
	{
		this->_integrator_sum = 0;
		_error = 0;
		this->_out = 0;
	}
};

} // namespace emb

