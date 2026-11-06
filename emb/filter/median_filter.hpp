#pragma once

#include <emb/math.hpp>

#include <algorithm>
#include <array>
#include <cstddef>

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
  std::array<value_type, window_size> window_{};
  std::size_t oldest_ = 0;
  bool full_ = false;
  value_type output_{};
public:
  constexpr median_filter() = default;

  constexpr explicit median_filter(value_type init_output)
  {
    reset(init_output);
  }

  constexpr void push(value_type input)
  {
    window_[oldest_] = input;
    if (++oldest_ == window_size) {
      oldest_ = 0;
      full_ = true;
    }
    if (!full_) return;
    auto values = window_;
    std::sort(values.begin(), values.end());
    output_ = values[window_size / 2];
  }

  constexpr value_type output() const
  {
    return output_;
  }

  constexpr void reset(value_type value)
  {
    window_.fill(value);
    full_ = true;
    output_ = value;
  }
};

} // namespace emb
