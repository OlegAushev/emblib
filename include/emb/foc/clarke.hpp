#pragma once

#include <emb/foc/pipe.hpp>
#include <emb/motorcontrol.hpp>

#include <array>

namespace emb {
namespace foc {

struct clarke {
  static constexpr vec_alphabeta operator()(std::array<float, 3> const& phase) {
    return clarke_transform(phase);
  }
};

} // namespace foc
} // namespace emb
