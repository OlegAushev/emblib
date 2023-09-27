#pragma once


#include <emblib/core.h>
#include <emblib/algorithm.h>
#include "float.h"


namespace emb {


#if defined(EMBLIB_C28X)
SCOPED_ENUM_DECLARE_BEGIN(controller_logic) {
    direct,
    inverse
} SCOPED_ENUM_DECLARE_END(controller_logic)
#elif defined(EMBLIB_STM32)
enum class controller_logic {
    direct,
    inverse
};
#endif


#if defined(EMBLIB_C28X)
template <controller_logic::enum_type Logic>
#elif defined(EMBLIB_STM32)
template <controller_logic Logic>
#endif
class abstract_picontroller : private emb::noncopyable {
protected:
    float _kp;              // proportional gain
    float _ki;              // integral gain
    float _ts;              // sampling period
    float _integrator_sum;  // integrator sum;
    float _out_min;         // PI output minimum limit
    float _out_max;         // PI output maximum limit
    float _out;             // PI output;

    static float _error(float ref, float meas);
public:
    abstract_picontroller(float kp, float ki, float ts, float out_min, float out_max)
            : _kp(kp)
            , _ki(ki)
            , _ts(ts)
            , _integrator_sum(0)
            , _out_min(out_min)
            , _out_max(out_max)
            , _out(0) {
    }

    virtual ~abstract_picontroller() {}
    virtual void push(float ref, float meas) = 0;
    virtual void reset() {
        _integrator_sum = 0;
        _out = 0;
    }
    float output() const { return _out; }
    void set_output_min(float val) { _out_min = val; }
    void set_output_max(float val) { _out_max = val; }
    float output_min() const { return _out_min; }
    float output_max() const { return _out_max; }

    void set_kp(float val) { _kp = val; }
    void set_ki(float val) { _ki = val; }
    float kp() const { return _kp; }
    float ki() const { return _ki; }
    float sum() const { return _integrator_sum; }
    void set_sampling_period(float val) { _ts = val; }
};


template<>
inline float abstract_picontroller<controller_logic::direct>::_error(float ref, float meas) { return ref - meas; }

template<>
inline float abstract_picontroller<controller_logic::inverse>::_error(float ref, float meas) { return meas - ref; }


#if defined(EMBLIB_C28X)
template <controller_logic::enum_type Logic>
#elif defined(EMBLIB_STM32)
template <controller_logic Logic>
#endif
class backcalc_picontroller : public abstract_picontroller<Logic> {
protected:
    float _kc;	// anti-windup gain
public:
    backcalc_picontroller(float kp, float ki, float ts, float kc, float out_min, float out_max)
            : abstract_picontroller<Logic>(kp, ki, ts, out_min, out_max)
            , _kc(kc) {
    }

    virtual void push(float ref, float meas) EMB_OVERRIDE {
        float error = abstract_picontroller<Logic>::_error(ref, meas);
        float out = emb::clamp(error * this->_kp + this->_integrator_sum, -FLT_MAX, FLT_MAX);

        if (out > this->_out_max) {
            this->_out = this->_out_max;
        } else if (out < this->_out_min) {
            this->_out = this->_out_min;
        } else {
            this->_out = out;
        }

        this->_integrator_sum = emb::clamp(this->_integrator_sum + this->_ki * this->_ts * error - _kc * (out - this->_out),
                -FLT_MAX, FLT_MAX);
    }
};


#if defined(EMBLIB_C28X)
template <controller_logic::enum_type Logic>
#elif defined(EMBLIB_STM32)
template <controller_logic Logic>
#endif
class clamping_picontroller : public abstract_picontroller<Logic> {
protected:
    float _error;
public:
    clamping_picontroller(float kp, float ki, float ts, float out_min, float out_max)
        : abstract_picontroller<Logic>(kp, ki, ts, out_min, out_max)
        , _error(0) {
    }

    virtual void push(float ref, float meas) EMB_OVERRIDE {
        float error = abstract_picontroller<Logic>::_error(ref, meas);
        float outp = error * this->_kp;
        float sum_i = (error + _error) * 0.5f * this->_ki * this->_ts + this->_integrator_sum;
        _error = error;
        float out = outp + sum_i;

        if (out > this->_out_max) {
            this->_out = this->_out_max;
            if (outp < this->_out_max) {
                this->_integrator_sum = this->_out_max - outp;
            }
        } else if (out < this->_out_min) {
            this->_out = this->_out_min;
            if (outp > this->_out_min) {
                this->_integrator_sum = this->_out_min - outp;
            }
        } else {
            this->_out = out;
            this->_integrator_sum = sum_i;
        }
    }

    virtual void reset() EMB_OVERRIDE {
        this->_integrator_sum = 0;
        _error = 0;
        this->_out = 0;
    }
};


} // namespace emb
