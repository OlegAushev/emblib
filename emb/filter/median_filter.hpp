#pragma once

#include <emb/container/circular_buffer.hpp>
#include <emb/math.hpp>

#include <algorithm>
#include <array>

namespace emb {

// The class template `median_filter` is a filter whose output is the median of
// the last `WindowSize` values pushed. Default construction leaves the window
// empty, and the filter waits for it: the output is `T{}` until the values
// pushed have filled the window. Construction with an initial output and
// `reset(value)` fill the window with copies of the value, which count as
// values pushed, so the output is that value and keeps it for the next
// `WindowSize / 2` pushes, and `reset(value)` ends the wait, if any. Passing
// zero as the initial output is not the same as passing none.
//
// The values in the window must not be NaN: with a NaN and two unequal numbers
// in it, `<` is not the strict weak ordering that `std::sort` requires, and the
// behavior is undefined. Each `push` into a full window sorts a local copy of
// it, which takes O(N log N) comparisons, where N is `WindowSize`.
template<typename T, std::size_t WindowSize>
  requires(emb::isodd(WindowSize))
class median_filter {
public:
  using value_type = T;
  static constexpr std::size_t window_size = WindowSize;
private:
  emb::circular_buffer<value_type, window_size> window_;
  value_type output_{};
public:
  constexpr median_filter() = default;

  constexpr explicit median_filter(value_type init_output)
  {
    reset(init_output);
  }

  constexpr void push(value_type input)
  {
    window_.push_back(input);
    if (!window_.full()) return;
    std::array<value_type, window_size> window_sorted = {};
    for (auto i = 0uz; i < window_.size(); ++i) {
      window_sorted[i] = window_[i];
    }
    std::sort(window_sorted.begin(), window_sorted.end());
    output_ = window_sorted[window_size / 2];
  }

  constexpr value_type output() const
  {
    return output_;
  }

  constexpr void reset(value_type value)
  {
    window_.fill(value);
    output_ = value;
  }
};

} // namespace emb
