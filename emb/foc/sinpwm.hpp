#pragma once

#include <emb/foc/clarke.hpp>
#include <emb/foc/types.hpp>
#include <emb/math.hpp>

#include <array>

namespace emb {
namespace foc {

inline three_phase<emb::unsigned_pu_f32> calculate_sinpwm(voltage_ab v_s,
                                                          float v_dc)
{
  voltage_abc const v = invclarke_transform(v_s);
  float const voltage_base = v_dc / 1.5f;
  return {.a = emb::unsigned_pu_f32(v.a / voltage_base),
          .b = emb::unsigned_pu_f32(v.b / voltage_base),
          .c = emb::unsigned_pu_f32(v.c / voltage_base)};
}

} // namespace foc
} // namespace emb
