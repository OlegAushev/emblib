#pragma once

#include <emb/foc/pipe.hpp>

namespace emb {
namespace foc {

template<typename T>
class store_to {
  T& dest_;
public:
  constexpr explicit store_to(T& dest) : dest_(dest) {}

  constexpr T operator()(T const& value) const {
    dest_ = value;
    return value;
  }
};

} // namespace foc
} // namespace emb
