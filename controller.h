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
#elif defined(EMBLIB_ARM)
enum class controller_logic {
    direct,
    inverse
};
#endif


#if defined(EMBLIB_C28X)
template <controller_logic::enum_type Logic>
#elif defined(EMBLIB_ARM)
template <controller_logic Logic>
#endif
class p_controller : private emb::noncopyable {
protected:
    float _kp;              // proportional gain
    float _lower_limit;     // output lower limit
    float _upper_limit;     // output upper limit
    float _out;             // output;

    static float _error(float ref, float meas);
public:
    p_controller(float kp, float lower_limit, float upper_limit)
            : _kp(kp)
            , _lower_limit(lower_limit)
            , _upper_limit(upper_limit)
            , _out(0) {}
    virtual ~p_controller() {}
    
    virtual void push(float ref, float meas) {
        float out = _kp * p_controller<Logic>::_error(ref, meas);
        _out = emb::clamp(out, _lower_limit, _upper_limit);
    }
    
    virtual void reset() { _out = 0; }
    float output() const { return _out; }
    void set_lower_limit(float value) { _lower_limit = value; }
    void set_upper_limit(float value) { _upper_limit = value; }
    float lower_limit() const { return _lower_limit; }
    float upper_limit() const { return _upper_limit; }

    void set_kp(float value) { _kp = value; }
    float kp() const { return _kp; }
};


template<>
inline float p_controller<controller_logic::direct>::_error(float ref, float meas) { return ref - meas; }

template<>
inline float p_controller<controller_logic::inverse>::_error(float ref, float meas) { return meas - ref; }


#if defined(EMBLIB_C28X)
template <controller_logic::enum_type Logic>
#elif defined(EMBLIB_ARM)
template <controller_logic Logic>
#endif
class abstract_pi_controller : private emb::noncopyable {
protected:
    float _kp;              // proportional gain
    float _ki;              // integral gain
    float _ts;              // sampling period
    float _out_i;           // integrator sum;
    float _lower_limit;     // output lower limit
    float _upper_limit;     // output upper limit
    float _out;             // output;

    static float _error(float ref, float meas);
public:
    abstract_pi_controller(float kp, float ki, float ts, float lower_limit, float upper_limit)
            : _kp(kp)
            , _ki(ki)
            , _ts(ts)
            , _out_i(0)
            , _lower_limit(lower_limit)
            , _upper_limit(upper_limit)
            , _out(0) {}

    virtual ~abstract_pi_controller() {}
    virtual void push(float ref, float meas) = 0;
    virtual void reset() {
        _out_i = 0;
        _out = 0;
    }
    float output() const { return _out; }
    void set_lower_limit(float value) { _lower_limit = value; }
    void set_upper_limit(float value) { _upper_limit = value; }
    float lower_limit() const { return _lower_limit; }
    float upper_limit() const { return _upper_limit; }

    void set_kp(float value) { _kp = value; }
    void set_ki(float value) { _ki = value; }
    float kp() const { return _kp; }
    float ki() const { return _ki; }
    float integral() const { return _out_i; }
    void set_sampling_period(float value) { _ts = value; }
};


template<>
inline float abstract_pi_controller<controller_logic::direct>::_error(float ref, float meas) { return ref - meas; }

template<>
inline float abstract_pi_controller<controller_logic::inverse>::_error(float ref, float meas) { return meas - ref; }


#if defined(EMBLIB_C28X)
template <controller_logic::enum_type Logic>
#elif defined(EMBLIB_ARM)
template <controller_logic Logic>
#endif
class backcalc_pi_controller : public abstract_pi_controller<Logic> {
protected:
    float _kc;	// anti-windup gain
public:
    backcalc_pi_controller(float kp, float ki, float ts, float kc, float lower_limit, float upper_limit)
            : abstract_pi_controller<Logic>(kp, ki, ts, lower_limit, upper_limit)
            , _kc(kc) {}

    virtual void push(float ref, float meas) EMB_OVERRIDE {
        float error = abstract_pi_controller<Logic>::_error(ref, meas);
        float out = emb::clamp(error * this->_kp + this->_out_i, -FLT_MAX, FLT_MAX);
        this->_out = emb::clamp(out, this->_lower_limit, this->_upper_limit);
        this->_out_i = emb::clamp(this->_out_i + this->_ki * this->_ts * error - _kc * (out - this->_out),
                                  -FLT_MAX,
                                  FLT_MAX);
    }
};


#if defined(EMBLIB_C28X)
template <controller_logic::enum_type Logic>
#elif defined(EMBLIB_ARM)
template <controller_logic Logic>
#endif
class clamping_pi_controller : public abstract_pi_controller<Logic> {
protected:
    float _error;
public:
    clamping_pi_controller(float kp, float ki, float ts, float lower_limit, float upper_limit)
        : abstract_pi_controller<Logic>(kp, ki, ts, lower_limit, upper_limit)
        , _error(0) {}

    virtual void push(float ref, float meas) EMB_OVERRIDE {
        float error = abstract_pi_controller<Logic>::_error(ref, meas);
        float out_p = error * this->_kp;
        float out_i = (error + _error) * 0.5f * this->_ki * this->_ts + this->_out_i;
        _error = error;
        float out = out_p + out_i;

        if (out > this->_upper_limit) {
            this->_out = this->_upper_limit;
            if (out_p < this->_upper_limit) {
                this->_out_i = this->_upper_limit - out_p;
            }
        } else if (out < this->_lower_limit) {
            this->_out = this->_lower_limit;
            if (out_p > this->_lower_limit) {
                this->_out_i = this->_lower_limit - out_p;
            }
        } else {
            this->_out = out;
            this->_out_i = out_i;
        }
    }

    virtual void reset() EMB_OVERRIDE {
        this->_out_i = 0;
        _error = 0;
        this->_out = 0;
    }
};


} // namespace emb
