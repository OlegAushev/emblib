#pragma once

#include <emb/container/circular_buffer.hpp>

#include <cstddef>
#include <utility>

namespace emb::filters {

// The class template `moving_average` is a filter whose output is the
// arithmetic mean of the last `WindowSize` inputs, one per call to `update`.
// Default construction leaves the window empty: the output is `T{}` until the
// next `update`, then the mean of the inputs since. Construction with an
// initial output and `reset(value)` fill the window with copies of the value,
// which count as inputs, so the output is that value and moves away from it as
// the copies leave the window; passing zero is thus not the same as passing
// nothing. `full()` tells whether the window is full, and `data()` returns it,
// oldest value first.
//
// The values are summed in `Sum`, by default the type of the sum of two `T`s,
// so that an integer `T` narrower than `int` is summed in `int`. For an
// integer `Sum`, the output is rounded toward zero, and `WindowSize` times any
// input must be representable in `Sum`. For a floating-point `Sum` or a
// `units::quantity`, the output carries the rounding errors of the sum, and a
// value much larger than the rest, an infinity or a NaN can affect it for up
// to `WindowSize - 1` updates after leaving the window. The errors do not
// accumulate over time.
template<typename T,
         std::size_t WindowSize,
         typename Sum = decltype(std::declval<T>() + std::declval<T>())>
  requires(WindowSize > 0)
class moving_average {
public:
  using value_type = T;
  using sum_type = Sum;
  using size_type = std::size_t;
  using underlying_type = emb::circular_buffer<value_type, WindowSize>;
  using divisor_type =
      decltype(std::declval<sum_type>() / std::declval<sum_type>());
  static constexpr std::size_t window_size = WindowSize;
private:
  underlying_type data_;
  sum_type sum_{};
  sum_type fresh_sum_{};
  size_type fresh_count_ = 0;
  value_type output_{};
public:
  constexpr moving_average() = default;

  constexpr explicit moving_average(value_type init_output)
  {
    reset(init_output);
  }

  constexpr void update(value_type input)
  {
    auto const addend = static_cast<sum_type>(input);
    if (!data_.full()) {
      data_.push_back(input);
      sum_ += addend;
    }
    else {
      sum_ = sum_ - static_cast<sum_type>(data_.front()) + addend;
      data_.push_back(input);
    }

    // Every `window_size` updates, the last `window_size` inputs summed from
    // zero replace `sum_`, so that its rounding errors do not accumulate.
    fresh_sum_ += addend;
    if (++fresh_count_ == window_size) {
      sum_ = fresh_sum_;
      fresh_sum_ = sum_type{0};
      fresh_count_ = 0;
    }
    output_ =
        static_cast<value_type>(sum_ / static_cast<divisor_type>(data_.size()));
  }

  constexpr value_type output() const
  {
    return output_;
  }

  constexpr bool full() const
  {
    return data_.full();
  }

  constexpr void reset(value_type value)
  {
    data_.fill(value);
    sum_ =
        static_cast<divisor_type>(window_size) * static_cast<sum_type>(value);
    fresh_sum_ = sum_type{0};
    fresh_count_ = 0;
    output_ = value;
  }

  constexpr underlying_type const& data() const
  {
    return data_;
  }
};

} // namespace emb::filters
