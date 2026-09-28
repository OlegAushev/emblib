#pragma once

#include <emb/container/circular_buffer.hpp>

#include <cstddef>
#include <utility>

namespace emb {

// The class template `moving_average_filter` is a filter whose output is the
// arithmetic mean of the last `WindowSize` values pushed. Construction,
// `reset()` and `set_output(value)` leave the window empty: the output is the
// initial output or `value` until the next `push`, then the mean of the values
// pushed since. `data()` returns the window, oldest value first.
//
// For an integer `T`, the output is rounded toward zero, and `WindowSize` times
// the largest magnitude of a value pushed must be representable in `T`. For a
// floating-point `T` or a `units::named_unit`, the output carries the rounding
// errors of the sum, and a value much larger than the rest, an infinity or a
// NaN can affect it for up to `WindowSize - 1` pushes after leaving the window.
// The errors do not accumulate over time.
template<typename T, std::size_t WindowSize>
class moving_average_filter {
public:
  using value_type = T;
  using size_type = std::size_t;
  using underlying_type = emb::circular_buffer<value_type, WindowSize>;
  using divider_type =
      decltype(std::declval<value_type>() / std::declval<value_type>());
  static constexpr std::size_t window_size = WindowSize;
private:
  underlying_type data_;
  value_type sum_;
  value_type fresh_sum_;
  size_type fresh_count_;
  value_type init_output_;
  value_type output_;
public:
  constexpr explicit moving_average_filter(
      value_type init_output = value_type{})
      : init_output_(init_output)
  {
    reset();
  }

  constexpr void push(value_type input)
  {
    if (!data_.full()) {
      data_.push_back(input);
      sum_ += input;
    }
    else {
      sum_ = sum_ - data_.front() + input;
      data_.push_back(input);
    }

    // Every `window_size` pushes, the last `window_size` values summed from
    // zero replace `sum_`, so that its rounding errors do not accumulate.
    fresh_sum_ += input;
    if (++fresh_count_ == window_size) {
      sum_ = fresh_sum_;
      fresh_sum_ = value_type{0};
      fresh_count_ = 0;
    }
    output_ = sum_ / static_cast<divider_type>(data_.size());
  }

  constexpr value_type output() const
  {
    return output_;
  }

  constexpr void set_output(value_type value)
  {
    data_.clear();
    sum_ = value_type{0};
    fresh_sum_ = value_type{0};
    fresh_count_ = 0;
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
