#pragma once

#include <emb/foc/types.hpp>
#include <emb/math.hpp>

#include <array>

namespace emb {
namespace foc {

constexpr float calculate_stator_current(current_ab const& i)
{
  return emb::sqrt(i.alpha * i.alpha + i.beta * i.beta);
}

constexpr float
calculate_dc_current(current_abc const& i,
                     three_phase<emb::unsigned_pu_f32> const& duty)
{
  return i.a * duty.a.value() + i.b * duty.b.value() + i.c * duty.c.value();
}

} // namespace foc
} // namespace emb
