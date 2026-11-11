#pragma once

#include <emb/algorithm.hpp>
#include <emb/math.hpp>

#include <array>
#include <cstddef>

namespace emb {

// The class template `median_filter` is a filter whose output is the median of
// the last `WindowSize` inputs, one per call to `update`. Default construction
// leaves the window empty, and the filter waits for it: the output is `T{}`
// until the inputs have filled the window, which `full()` tells. Construction
// with an initial output and `reset(value)` fill the window with copies of the
// value, which count as inputs, so the output is that value and keeps it for
// the next `WindowSize / 2` updates, and `reset(value)` ends the wait, if any.
// Passing zero as the initial output is not the same as passing none.
//
// Once the window is full, each `update` passes it to `emb::median`, and the
// contract of that function applies: the values in the window must not be NaN.
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

  constexpr void update(value_type input)
  {
    window_[oldest_] = input;
    if (++oldest_ == window_size) {
      oldest_ = 0;
      full_ = true;
    }
    if (!full_) return;
    output_ = emb::median(window_);
  }

  constexpr value_type output() const
  {
    return output_;
  }

  constexpr bool full() const
  {
    return full_;
  }

  constexpr void reset(value_type value)
  {
    window_.fill(value);
    full_ = true;
    output_ = value;
  }
};

} // namespace emb
