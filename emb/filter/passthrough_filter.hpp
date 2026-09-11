#pragma once

namespace emb {

// Identity filter: holds the most recent value without smoothing. Useful where
// a filtering stage is required by interface but no actual filtering is wanted.
template<typename T>
class passthrough_filter {
public:
  using value_type = T;
private:
  value_type value_{};
public:
  constexpr passthrough_filter() = default;

  constexpr explicit passthrough_filter(value_type init) : value_(init)
  {
  }

  constexpr void push(value_type input)
  {
    value_ = input;
  }

  constexpr value_type output() const
  {
    return value_;
  }
};

} // namespace emb
