#pragma once

#include <algorithm>
#include <emblib/algorithm.hpp>
#include <emblib/circular_buffer.hpp>
#include <emblib/core.hpp>
#include <vector>

namespace emb {

template<typename T, size_t Capacity>
class moving_average_filter {
private:
  emb::circular_buffer<T, Capacity> data_;
  T sum_;
  T init_output_;
  T output_;
public:
  moving_average_filter(T const& init_output = T())
      : init_output_(init_output) {
    reset();
  }

  void push(T const& input_v) {
    if (!data_.full()) {
      data_.push_back(input_v);
      sum_ += input_v;
    } else {
      sum_ = sum_ - data_.front() + input_v;
      data_.push_back(input_v);
    }
    output_ = sum_ / data_.size();
  }

  T output() const { return output_; }

  void set_output(T const& output_v) {
    data_.clear();
    sum_ = 0;
    output_ = output_v;
  }

  void reset() { set_output(init_output_); }
};

} // namespace emb
