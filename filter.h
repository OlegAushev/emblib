#pragma once


#include <emblib/core.h>
#include <emblib/algorithm.h>
#include <emblib/array.h>
#include <emblib/circular_buffer.h>
#include <algorithm>
#include <float.h>


namespace emb {


template<typename T>
class filter {
public:
    filter() {}
    virtual ~filter() {}

    virtual void push(T input_value) = 0;
    virtual T output() const = 0;
    virtual void set_output(T value) = 0;
    virtual void reset() = 0;
};


template<typename T, size_t WindowSize>
class movavg_filter : public filter<T>, private emb::noncopyable {
private:
    size_t _size;
    T* _window;
    size_t _index;
    T _sum;
    bool _heap_used;
public:
    movavg_filter()
            : _size(WindowSize)
            , _window(new T[WindowSize])
            , _index(0)
            , _sum(0)
            , _heap_used(true) {
        reset();
    }

    movavg_filter(emb::array<T, WindowSize>& data_array)
            : _size(WindowSize)
            , _window(data_array.begin())
            , _index(0)
            , _sum(T(0))
            , _heap_used(false) {
        reset();
    }

    ~movavg_filter() {
        if (_heap_used == true) {
            delete[] _window;
        }
    }

    virtual void push(T input_value) EMB_OVERRIDE {
        _sum = _sum + input_value - _window[_index];
        _window[_index] = input_value;
        _index = (_index + 1) % _size;
    }

    virtual T output() const EMB_OVERRIDE { return _sum / T(_size); }

    virtual void set_output(T value) EMB_OVERRIDE {
        for (size_t i = 0; i < _size; ++i) {
            _window[i] = value;
        }
        _index = 0;
        _sum = value * T(_size);
    }

    virtual void reset() EMB_OVERRIDE { set_output(0); }

    size_t size() const { return _size; }

    void resize(size_t size) {
        if (size == 0) {
            return;
        }
        if (size > WindowSize) {
            _size = WindowSize;
            reset();
            return;
        }
        _size = size;
        reset();
    }
};


template<typename T, size_t WindowSize>
class med_filter : public filter<T> {
private:
    circular_buffer<T, WindowSize> _window;
    T _out;
public:
    med_filter() {
        EMB_STATIC_ASSERT((WindowSize % 2) == 1);
        reset();
    }

    virtual void push(T input_value) EMB_OVERRIDE {
        _window.push_back(input_value);
        emb::array<T, WindowSize> window_sorted;
        emb::copy(_window.begin(), _window.end(), window_sorted.begin());
        std::sort(window_sorted.begin(), window_sorted.end());
        _out = window_sorted[WindowSize/2];
    }

    virtual T output() const EMB_OVERRIDE { return _out; }

    virtual void set_output(T value) EMB_OVERRIDE {
        _window.fill(value);
        _out = value;
    }

    virtual void reset() EMB_OVERRIDE { set_output(0); }
};


template<typename T>
class exp_filter : public filter<T> {
private:
    float _sampling_period;
    float _time_constant;
    float _smooth_factor;
    T _out;
    T _out_prev;
public:
    exp_filter()
            : _sampling_period(0)
            , _time_constant(FLT_MAX)
            , _smooth_factor(0) {
        reset();
    }

    exp_filter(float sampling_period, float time_constant) {
        init(sampling_period, time_constant);
        reset();
    }

    virtual void push(T input_value) EMB_OVERRIDE {
        _out = _out_prev + _smooth_factor * (input_value - _out_prev);
        _out_prev = _out;
    }

    virtual T output() const EMB_OVERRIDE { return _out; }

    virtual void set_output(T value) EMB_OVERRIDE {
        _out = value;
        _out_prev = value;
    }

    virtual void reset() EMB_OVERRIDE { set_output(0); }

    void init(float sampling_period, float time_constant) {
        _sampling_period = sampling_period;
        _time_constant = time_constant;
        _smooth_factor = emb::clamp(sampling_period/time_constant, 0.f, 1.f);
    }

    void set_sampling_period(float value) {
        _sampling_period = value;
        _smooth_factor = emb::clamp(_sampling_period/_time_constant, 0.f, 1.f);
    }
};


template<typename T, size_t WindowSize>
class expmed_filter : public filter<T> {
private:
    circular_buffer<T, WindowSize> _window;
    float _sampling_period;
    float _time_constant;
    float _smooth_factor;
    T _out;
    T _out_prev;
public:
    expmed_filter()
            : _sampling_period(0)
            , _time_constant(FLT_MAX)
            , _smooth_factor(0) {
        EMB_STATIC_ASSERT((WindowSize % 2) == 1);
        reset();
    }

    expmed_filter(float sampling_period, float time_constant) {
        EMB_STATIC_ASSERT((WindowSize % 2) == 1);
        init(sampling_period, time_constant);
        reset();
    }

    virtual void push(T input_value) EMB_OVERRIDE {
        _window.push_back(input_value);
        emb::array<T, WindowSize> window_sorted;
        emb::copy(_window.begin(), _window.end(), window_sorted.begin());
        std::sort(window_sorted.begin(), window_sorted.end());
        input_value = window_sorted[WindowSize/2];

        _out = _out_prev + _smooth_factor * (input_value - _out_prev);
        _out_prev = _out;
    }

    virtual T output() const EMB_OVERRIDE { return _out; }

    virtual void set_output(T value) EMB_OVERRIDE {
        _window.fill(value);
        _out = value;
        _out_prev = value;
    }

    virtual void reset() EMB_OVERRIDE { set_output(0); }
    
    void init(float sampling_period, float time_constant) {
        _sampling_period = sampling_period;
        _time_constant = time_constant;
        _smooth_factor = emb::clamp(sampling_period/time_constant, 0.f, 1.f);
    }

    void set_sampling_period(float value) {
        _sampling_period = value;
        _smooth_factor = emb::clamp(_sampling_period/_time_constant, 0.f, 1.f);
    }
};


template<typename T>
class ramp_filter : public filter<T> {
private:
    float _update_period;
    float _slope;
    float _step;

    T _ref;
    T _out;
public:
    ramp_filter()
            : _update_period(0)
            , _slope(0)
            , _step(0) {
        reset();
    }

    ramp_filter(float update_period, float slope) {
        init(update_period, slope);
        reset();
    }

    virtual void push(T input_value) EMB_OVERRIDE {
        _ref = input_value;
    }

    virtual T output() const EMB_OVERRIDE { return _out; }
    
    virtual void set_output(T value) EMB_OVERRIDE {
        _ref = value;
        _out = value;
    }

    virtual void reset() EMB_OVERRIDE { set_output(0); }

    void init(float update_period, float slope) {
        _update_period = update_period;
        _slope = slope;
        _step = emb::clamp(update_period * slope, -FLT_MAX, FLT_MAX);
    }

    void update() {
        if (_out < _ref) {
            _out = std::min(_out + _step, _ref);
        } else {
            _out = std::max(_out - _step, _ref);
        }
    }
};


} // namespace emb
