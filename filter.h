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


template <typename T, size_t WindowSize>
class moving_avg_filter : public filter_interface<T>, public emb::noncopyable {
private:
	size_t _size;
	T* _window;
	size_t _index;
	T _sum;
	bool _heap_used;
public:
	moving_avg_filter()
			: _size(WindowSize)
			, _window(new T[WindowSize])
			, _index(0)
			, _sum(0)
			, _heap_used(true) {
		reset();
	}

	moving_avg_filter(emb::array<T, WindowSize>& data_array)
			: _size(WindowSize)
			, _window(data_array.data)
			, _index(0)
			, _sum(T(0))
			, _heap_used(false) {
		reset();
	}

	~moving_avg_filter() {
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
		for (size_t i = 0; i < _size; ++i) {
			_window[i] = value;
		}
		_index = 0;
		_sum = value * _size;
	}

	virtual void reset() { set_output(0); }

	int size() const { return _size; }

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


template <typename T, size_t WindowSize>
class median_filter : public filter_interface<T> {
private:
	circular_buffer<T, WindowSize> _window;
	T _out;
public:
	median_filter() {
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
class exponential_filter : public filter_interface<T> {
private:
	float _smooth_factor;
	T _out;
	T _outPrev;
public:
	exponential_filter()
			: _smooth_factor(0) {
		reset();
	}

	exponential_filter(float smooth_factor)
			: _smooth_factor(smooth_factor) {
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

	void set_smooth_factor(float smooth_factor) { _smooth_factor = smooth_factor; }
};


template <typename T, size_t WindowSize>
class exponential_median_filter : public filter_interface<T> {
private:
	circular_buffer<T, WindowSize> _window;
	float _smooth_factor;
	T _out;
	T _out_prev;
public:
	exponential_median_filter()
			: _smooth_factor(0) {
		EMB_STATIC_ASSERT((WindowSize % 2) == 1);
		reset();
	}

	exponential_median_filter(float smooth_factor)
			: _smooth_factor(smooth_factor) {
		EMB_STATIC_ASSERT((WindowSize % 2) == 1);
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
	
	void set_smooth_factor(float smooth_factor) { _smooth_factor = smooth_factor; }
};

} // namespace emb

