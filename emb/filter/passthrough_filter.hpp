#pragma once

namespace emb {

// Identity filter: holds the most recent value without smoothing. Useful where
// a filtering stage is required by interface but no actual filtering is wanted.
template<typename T>
class passthrough_filter {
public:
  using value_type = T;
  using reference = value_type&;
  using const_reference = value_type const&;
private:
  value_type value_{};
public:
  constexpr passthrough_filter() = default;

  constexpr explicit passthrough_filter(value_type const& init) : value_(init) {}

  constexpr void push(value_type const& input_v) {
    value_ = input_v;
  }

  constexpr const_reference output() const {
    return value_;
  }
};

} // namespace emb
