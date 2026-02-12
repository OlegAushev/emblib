#pragma once

#include <algorithm>
#include <emb/algorithm.hpp>
#include <emb/circular_buffer.hpp>
#include <emb/core.hpp>
#include <emb/math.hpp>
#include <emb/units.hpp>

namespace emb {
namespace filter {

template<typename T, size_t WindowSize>
class moving_average {
public:
  using value_type = T;
  using size_type = size_t;
  using reference = value_type&;
  using const_reference = value_type const&;
  using underlying_type = emb::circular_buffer<value_type, WindowSize>;
  using divider_type =
      decltype(std::declval<value_type>() / std::declval<value_type>());
  static constexpr size_t window_size = WindowSize;
private:
  underlying_type data_;
  value_type sum_;
  value_type init_output_;
  value_type output_;
public:
  constexpr explicit moving_average(
      value_type const& init_output = value_type{})
      : init_output_(init_output) {
    reset();
  }

  constexpr void push(value_type const& input_v) {
    if (!data_.full()) {
      data_.push_back(input_v);
      sum_ += input_v;
    } else {
      sum_ = sum_ - data_.front() + input_v;
      data_.push_back(input_v);
    }
    output_ = sum_ / static_cast<divider_type>(data_.size());
  }

  constexpr const_reference output() const { return output_; }

  constexpr void set_output(value_type const& output_v) {
    data_.clear();
    sum_ = value_type{0};
    output_ = output_v;
  }

  constexpr void reset() { set_output(init_output_); }

  constexpr underlying_type const& data() const { return data_; }
};

template<typename T>
struct is_moving_average_filter_type : std::false_type {};

template<typename T, size_t WindowSize>
struct is_moving_average_filter_type<moving_average<T, WindowSize>>
    : std::true_type {};

template<typename T>
concept moving_average_filter_type = is_moving_average_filter_type<T>::value;

}
} // namespace emb
