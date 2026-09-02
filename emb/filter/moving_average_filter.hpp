#pragma once

#include <algorithm>
#include <emb/algorithm.hpp>
#include <emb/container/circular_buffer.hpp>
#include <emb/math.hpp>
#include <emb/units.hpp>

namespace emb {

template<typename T, std::size_t WindowSize>
class moving_average_filter {
public:
  using value_type = T;
  using size_type = std::size_t;
  using reference = value_type&;
  using const_reference = value_type const&;
  using underlying_type = emb::circular_buffer<value_type, WindowSize>;
  using divider_type = decltype(std::declval<value_type>()
                                / std::declval<value_type>());
  static constexpr std::size_t window_size = WindowSize;
private:
  underlying_type data_;
  value_type sum_;
  value_type init_output_;
  value_type output_;
public:
  constexpr explicit moving_average_filter(
      value_type const& init_output = value_type{})
      : init_output_(init_output)
  {
    reset();
  }

  constexpr void push(value_type const& input)
  {
    if (!data_.full()) {
      data_.push_back(input);
      sum_ += input;
    }
    else {
      sum_ = sum_ - data_.front() + input;
      data_.push_back(input);
    }
    output_ = sum_ / static_cast<divider_type>(data_.size());
  }

  constexpr const_reference output() const
  {
    return output_;
  }

  constexpr void set_output(value_type const& value)
  {
    data_.clear();
    sum_ = value_type{0};
    output_ = value;
  }

  constexpr void reset()
  {
    set_output(init_output_);
  }

  constexpr underlying_type const& data() const
  {
    return data_;
  }
};

} // namespace emb
