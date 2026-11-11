#pragma once

namespace emb {

// The class template `passthrough_filter` is a filter whose output is its last
// input. Before the first `update`, the output is the value passed to the
// constructor, or `T{}` if none was passed. Where no filtering is wanted, it
// serves as the filter that `sensor::singlechannel` requires.
template<typename T>
class passthrough_filter {
public:
  using value_type = T;
private:
  value_type value_{};
public:
  constexpr passthrough_filter() = default;

  constexpr explicit passthrough_filter(value_type init) : value_(init) {}

  constexpr void update(value_type input)
  {
    value_ = input;
  }

  constexpr value_type output() const
  {
    return value_;
  }
};

} // namespace emb
