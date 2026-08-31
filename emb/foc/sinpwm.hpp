#pragma once

#include <emb/foc/types.hpp>
#include <emb/foc/clarke.hpp>
#include <emb/math.hpp>

#include <array>

namespace emb {
namespace foc {

inline std::array<emb::unsigned_pu_f32, 3>
calculate_sinpwm(voltage_ab v_s, float v_dc) {
  voltage_abc const v = invclarke_transform(v_s);
  float const voltage_base = v_dc / 1.5f;
  return {
      emb::unsigned_pu_f32(v.a / voltage_base),
      emb::unsigned_pu_f32(v.b / voltage_base),
      emb::unsigned_pu_f32(v.c / voltage_base)
  };
}

} // namespace foc
} // namespace emb
