#pragma once


#include <emblib_c28x/core.h>
#include <emblib_c28x/algorithm.h>
#include <emblib_c28x/array.h>
#include <emblib_c28x/circularbuffer.h>
#include <algorithm>


namespace emb {

template <typename T>
class filter_interface {
public:
    filter_interface() {}
    virtual ~filter_interface() {}

    virtual void update(T value) = 0;
    virtual T output() const = 0;
    virtual void set_output(T value) = 0;
    virtual void reset() = 0;
};


template <typename T, int WindowSize>
class movavg_filter : public filter_interface<T>, private emb::noncopyable {
private:
    int _size;
    T* _window;
    int _index;
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
            , _window(data_array.data)
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

    virtual void update(T value) {
        _sum = _sum + value - _window[_index];
        _window[_index] = value;
        _index = (_index + 1) % _size;
    }

    virtual T output() const { return _sum / _size; }

    virtual void set_output(T value) {
        for (int i = 0; i < _size; ++i) {
            _window[i] = value;
        }
        _index = 0;
        _sum = value * _size;
    }

    virtual void reset() { set_output(0); }

    int size() const { return _size; }

    void resize(int size) {
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


template <typename T, int WindowSize>
class med_filter : public filter_interface<T> {
private:
    circular_buffer<T, WindowSize> _window;
    T _out;
public:
    med_filter() {
        EMB_STATIC_ASSERT((WindowSize % 2) == 1);
        reset();
    }

    virtual void update(T value) {
        _window.push(value);
        emb::array<T, WindowSize> window_sorted;
        emb::copy(_window.begin(), _window.end(), window_sorted.begin());
        std::sort(window_sorted.begin(), window_sorted.end());
        _out = window_sorted[WindowSize/2];
    }

    virtual T output() const { return _out; }

    virtual void set_output(T value) {
        _window.fill(value);
        _out = value;
    }

    virtual void reset() { set_output(0); }
};


template <typename T>
class exp_filter : public filter_interface<T> {
private:
    float _smooth_factor;
    T _out;
    T _outPrev;
public:
    exp_filter()
            : _smooth_factor(0) {
        reset();
    }

    exp_filter(float sampling_period, float time_constant) {
        _smooth_factor = emb::clamp(sampling_period/time_constant, 0.f, 1.f);
        reset();
    }

    virtual void update(T value) {
        _out = _outPrev + _smooth_factor * (value - _outPrev);
        _outPrev = _out;
    }

    virtual T output() const { return _out; }

    virtual void set_output(T value) {
        _out = value;
        _outPrev = value;
    }

    virtual void reset() { set_output(0); }

    void init(float sampling_period, float time_constant) {
        _smooth_factor = emb::clamp(sampling_period/time_constant, 0.f, 1.f);
    }
};


template <typename T, int WindowSize>
class expmed_filter : public filter_interface<T> {
private:
    circular_buffer<T, WindowSize> _window;
    float _smooth_factor;
    T _out;
    T _out_prev;
public:
    expmed_filter()
            : _smooth_factor(0) {
        EMB_STATIC_ASSERT((WindowSize % 2) == 1);
        reset();
    }

    expmed_filter(float sampling_period, float time_constant) {
        EMB_STATIC_ASSERT((WindowSize % 2) == 1);
        _smooth_factor = emb::clamp(sampling_period/time_constant, 0.f, 1.f);
        reset();
    }

    virtual void update(T value) {
        _window.push(value);
        emb::array<T, WindowSize> window_sorted;
        emb::copy(_window.begin(), _window.end(), window_sorted.begin());
        std::sort(window_sorted.begin(), window_sorted.end());
        value = window_sorted[WindowSize/2];

        _out = _out_prev + _smooth_factor * (value - _out_prev);
        _out_prev = _out;
    }

    virtual T output() const { return _out; }

    virtual void set_output(T value) {
        _window.fill(value);
        _out = value;
        _out_prev = value;
    }

    virtual void reset() { set_output(0); }
    
    void init(float sampling_period, float time_constant) {
        _smooth_factor = emb::clamp(sampling_period/time_constant, 0.f, 1.f);
    }
};

} // namespace emb

