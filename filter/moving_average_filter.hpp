#pragma once

#include <algorithm>
#include <emblib/algorithm.hpp>
#include <emblib/array.hpp>
#include <emblib/circular_buffer.hpp>
#include <emblib/core.hpp>
#include <emblib/noncopyable.hpp>
#include <float.h>

namespace emb {

template<typename T, size_t WindowSize>
class movavg_filter {
private:
  size_t size_;
  T* window_;
  size_t index_;
  T sum_;
  bool heap_used_;
public:
  movavg_filter()
      : size_(WindowSize),
        window_(new T[WindowSize]),
        index_(0),
        sum_(0),
        heap_used_(true) {
    reset();
  }

  movavg_filter(emb::array<T, WindowSize>& data_array)
      : size_(WindowSize),
        window_(data_array.begin()),
        index_(0),
        sum_(T(0)),
        heap_used_(false) {
    reset();
  }

  ~movavg_filter() {
    if (heap_used_ == true) {
      delete[] window_;
    }
  }

  void push(T input_value) {
    sum_ = sum_ + input_value - window_[index_];
    window_[index_] = input_value;
    index_ = (index_ + 1) % size_;
  }

  T output() const { return sum_ / T(size_); }

  void set_output(T value) {
    for (size_t i = 0; i < size_; ++i) {
      window_[i] = value;
    }
    index_ = 0;
    sum_ = value * T(size_);
  }

  void reset() { set_output(T(0)); }

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

} // namespace emb
