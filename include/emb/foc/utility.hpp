#pragma once

#include <emb/math.hpp>

#include <array>

namespace emb {
namespace foc {

constexpr float calculate_dc_current(
    std::array<float, 3> i_ph,
    std::array<emb::unsigned_pu, 3> d
) {
  return i_ph[0] * d[0].value()
       + i_ph[1] * d[1].value()
       + i_ph[2] * d[2].value();
}

} // namespace foc
} // namespace emb
