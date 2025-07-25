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
class median_filter {
private:
  circular_buffer<T, WindowSize> window_;
  T out_;
public:
  median_filter() {
    EMB_STATIC_ASSERT((WindowSize % 2) == 1);
    reset();
  }

  void push(T input_value) {
    window_.push_back(input_value);
    emb::array<T, WindowSize> window_sorted = {};
    std::copy(window_.begin(), window_.end(), window_sorted.begin());
    std::sort(window_sorted.begin(), window_sorted.end());
    out_ = window_sorted[WindowSize / 2];
  }

  T output() const { return out_; }

  void set_output(T value) {
    window_.fill(value);
    out_ = value;
  }

  void reset() { set_output(T(0)); }
};

} // namespace emb
