#pragma once

#include <emb/foc/types.hpp>
#include <emb/math.hpp>

#include <array>

namespace emb {
namespace foc {

constexpr float calculate_stator_current(current_ab const& i) {
  return emb::sqrt(i.alpha * i.alpha + i.beta * i.beta);
}

constexpr float calculate_dc_current(
    current_abc const& i,
    std::array<emb::unsigned_pu_f32, 3> const& duty
) {
  return i.a * duty[0].value() + i.b * duty[1].value() + i.c * duty[2].value();
}

} // namespace foc
} // namespace emb
