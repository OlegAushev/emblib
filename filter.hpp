#pragma once

#include <emblib/core.hpp>
#include <emblib/algorithm.hpp>
#include <emblib/array.hpp>
#include <emblib/circular_buffer.hpp>
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
    size_t size_;
    T* window_;
    size_t index_;
    T sum_;
    bool heap_used_;
public:
    movavg_filter()
            : size_(WindowSize)
            , window_(new T[WindowSize])
            , index_(0)
            , sum_(0)
            , heap_used_(true) {
        reset();
    }

    movavg_filter(emb::array<T, WindowSize>& data_array)
            : size_(WindowSize)
            , window_(data_array.begin())
            , index_(0)
            , sum_(T(0))
            , heap_used_(false) {
        reset();
    }

    ~movavg_filter() {
        if (heap_used_ == true) {
            delete[] window_;
        }
    }

    virtual void push(T input_value) EMB_OVERRIDE {
        sum_ = sum_ + input_value - window_[index_];
        window_[index_] = input_value;
        index_ = (index_ + 1) % size_;
    }

    virtual T output() const EMB_OVERRIDE { return sum_ / T(size_); }

    virtual void set_output(T value) EMB_OVERRIDE {
        for (size_t i = 0; i < size_; ++i) {
            window_[i] = value;
        }
        index_ = 0;
        sum_ = value * T(size_);
    }

    virtual void reset() EMB_OVERRIDE { set_output(0); }

    size_t size() const { return size_; }

    void resize(size_t size) {
        if (size == 0) {
            return;
        }
        if (size > WindowSize) {
            size_ = WindowSize;
            reset();
            return;
        }
        size_ = size;
        reset();
    }
};

template<typename T, size_t WindowSize>
class med_filter : public filter<T> {
private:
    circular_buffer<T, WindowSize> window_;
    T out_;
public:
    med_filter() {
        EMB_STATIC_ASSERT((WindowSize % 2) == 1);
        reset();
    }

    virtual void push(T input_value) EMB_OVERRIDE {
        window_.push_back(input_value);
        emb::array<T, WindowSize> window_sorted;
        std::copy(window_.begin(), window_.end(), window_sorted.begin());
        std::sort(window_sorted.begin(), window_sorted.end());
        out_ = window_sorted[WindowSize/2];
    }

    virtual T output() const EMB_OVERRIDE { return out_; }

    virtual void set_output(T value) EMB_OVERRIDE {
        window_.fill(value);
        out_ = value;
    }

    virtual void reset() EMB_OVERRIDE { set_output(0); }
};

template<typename T>
class exp_filter : public filter<T> {
private:
    float sampling_period_;
    float time_constant_;
    float smooth_factor_;
    T out_;
public:
    exp_filter()
            : sampling_period_(0)
            , time_constant_(FLT_MAX)
            , smooth_factor_(0) {
        reset();
    }

    exp_filter(float sampling_period, float time_constant) {
        init(sampling_period, time_constant);
        reset();
    }

    virtual void push(T input_value) EMB_OVERRIDE {
        out_ = out_ + smooth_factor_ * (input_value - out_);
    }

    virtual T output() const EMB_OVERRIDE { return out_; }

    virtual void set_output(T value) EMB_OVERRIDE { out_ = value; }

    virtual void reset() EMB_OVERRIDE { set_output(0); }

    void init(float sampling_period, float time_constant) {
        sampling_period_ = sampling_period;
        time_constant_ = time_constant;
        smooth_factor_ = emb::clamp(sampling_period/time_constant, 0.f, 1.f);
    }

    void set_sampling_period(float value) {
        sampling_period_ = value;
        smooth_factor_ = emb::clamp(sampling_period_/time_constant_, 0.f, 1.f);
    }

    float smooth_factor() const { return smooth_factor_; }
};

template<typename T, size_t WindowSize>
class expmed_filter : public filter<T> {
private:
    circular_buffer<T, WindowSize> _window;
    float sampling_period_;
    float time_constant_;
    float smooth_factor_;
    T out_;
public:
    expmed_filter()
            : sampling_period_(0)
            , time_constant_(FLT_MAX)
            , smooth_factor_(0) {
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
        std::copy(_window.begin(), _window.end(), window_sorted.begin());
        std::sort(window_sorted.begin(), window_sorted.end());
        input_value = window_sorted[WindowSize/2];

        out_ = out_ + smooth_factor_ * (input_value - out_);
    }

    virtual T output() const EMB_OVERRIDE { return out_; }

    virtual void set_output(T value) EMB_OVERRIDE {
        _window.fill(value);
        out_ = value;
    }

    virtual void reset() EMB_OVERRIDE { set_output(0); }

    void init(float sampling_period, float time_constant) {
        sampling_period_ = sampling_period;
        time_constant_ = time_constant;
        smooth_factor_ = emb::clamp(sampling_period/time_constant, 0.f, 1.f);
    }

    void set_sampling_period(float value) {
        sampling_period_ = value;
        smooth_factor_ = emb::clamp(sampling_period_/time_constant_, 0.f, 1.f);
    }

    float smooth_factor() const { return smooth_factor_; }
};

template<typename T>
class ramp_filter : public filter<T> {
private:
    float update_period_;
    T slope_;
    T step_;

    T ref_;
    T out_;
public:
    ramp_filter()
            : update_period_(0)
            , slope_(T(0))
            , step_(T(0)) {
        reset();
    }

    ramp_filter(float update_period, T slope) {
        init(update_period, slope);
        reset();
    }

    virtual void push(T input_value) EMB_OVERRIDE {
        ref_ = input_value;
    }

    virtual T output() const EMB_OVERRIDE { return out_; }

    virtual void set_output(T value) EMB_OVERRIDE {
        ref_ = value;
        out_ = value;
    }

    virtual void reset() EMB_OVERRIDE { set_output(T(0)); }

    void init(float update_period, T slope) {
        assert(update_period > 0);
        assert(slope > T(0));
        update_period_ = update_period;
        slope_ = slope;
        step_ = emb::clamp(update_period * slope, T(-FLT_MAX), T(FLT_MAX));
    }

    void update() {
        if (out_ < ref_) {
            out_ = std::min(out_ + step_, ref_);
        } else {
            out_ = std::max(out_ - step_, ref_);
        }
    }

    bool steady() const { return out_ == ref_; }
};

} // namespace emb
