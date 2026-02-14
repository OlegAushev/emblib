#pragma once

#include <emb/foc/pipe.hpp>
#include <emb/motorcontrol.hpp>

#include <array>

namespace emb {
namespace foc {

struct clarke_fn {
  static constexpr vec_alphabeta operator()(std::array<float, 3> const& phase) {
    return clarke_transform(phase);
  }
};

inline constexpr clarke_fn clarke;

} // namespace foc
} // namespace emb
