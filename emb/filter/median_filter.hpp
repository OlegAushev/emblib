#pragma once

#include <emb/container/circular_buffer.hpp>
#include <emb/math.hpp>

#include <algorithm>
#include <array>

namespace emb {

template<typename T, std::size_t WindowSize>
  requires(emb::isodd(WindowSize))
class median_filter {
public:
  using value_type = T;
  using reference = value_type&;
  using const_reference = value_type const&;
  static constexpr std::size_t window_size = WindowSize;
private:
  emb::circular_buffer<value_type, window_size> window_;
  value_type init_output_;
  value_type output_;
public:
  constexpr median_filter(value_type const& init_output = value_type())
      : init_output_(init_output) {
    reset();
  }

  constexpr void push(value_type const& input_v) {
    window_.push_back(input_v);
    std::array<value_type, window_size> window_sorted = {};
    for (auto i = 0uz; i < window_.size(); ++i) {
      window_sorted[i] = window_[i];
    }
    std::sort(window_sorted.begin(), window_sorted.end());
    output_ = window_sorted[window_size / 2];
  }

  constexpr const_reference output() const {
    return output_;
  }

  constexpr void set_output(value_type const& output_v) {
    window_.fill(output_v);
    output_ = output_v;
  }

  constexpr void reset() {
    set_output(init_output_);
  }
};

} // namespace emb
