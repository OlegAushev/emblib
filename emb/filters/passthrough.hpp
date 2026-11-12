#pragma once

namespace emb::filters {

// The class template `passthrough` is a filter whose output is its last
// input. Before the first `update`, the output is the value passed to the
// constructor, or `T{}` if none was passed. Where no filtering is wanted, it
// serves as the filter that `sensors::basic` requires.
template<typename T>
class passthrough {
public:
  using value_type = T;
private:
  value_type value_{};
public:
  constexpr passthrough() = default;

  constexpr explicit passthrough(value_type init) : value_(init) {}

  constexpr void update(value_type input)
  {
    value_ = input;
  }

  constexpr value_type output() const
  {
    return value_;
  }
};

} // namespace emb::filters
