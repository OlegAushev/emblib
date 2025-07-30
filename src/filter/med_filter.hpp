#pragma once

#include <algorithm>
#include <emb/algorithm.hpp>
#include <emb/array.hpp>
#include <emb/circular_buffer.hpp>
#include <emb/core.hpp>

namespace emb {

template<typename T, size_t WindowSize>
class med_filter {
private:
  emb::circular_buffer<T, WindowSize> window_;
  T init_output_;
  T output_;
public:
  med_filter(T const& init_output = T()) : init_output_(init_output) {
    EMB_STATIC_ASSERT((WindowSize % 2) == 1);
    reset();
  }

  void push(T const& input_v) {
    window_.push_back(input_v);
    emb::array<T, WindowSize> window_sorted = {};
    std::copy(window_.begin(), window_.end(), window_sorted.begin());
    std::sort(window_sorted.begin(), window_sorted.end());
    output_ = window_sorted[WindowSize / 2];
  }

  T output() const { return output_; }

  void set_output(T const& output_v) {
    window_.fill(output_v);
    output_ = output_v;
  }

  void reset() { set_output(init_output_); }
};

} // namespace emb
