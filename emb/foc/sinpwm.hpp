#pragma once

#include <emb/foc/types.hpp>
#include <emb/foc/clarke.hpp>
#include <emb/math.hpp>

#include <array>

namespace emb {
namespace foc {

inline std::array<emb::unsigned_pu, 3>
calculate_sinpwm(vec_ab v_s, float v_dc) {
  std::array<float, 3> voltages = invclarke_transform(v_s);
  float const voltage_base = v_dc / 1.5f;
  std::array<emb::unsigned_pu, 3> duty_cycles;

  for (size_t i = 0; i < 3; ++i) {
    duty_cycles[i] = emb::unsigned_pu(voltages[i] / voltage_base);
  }

  return duty_cycles;
}

} // namespace foc
} // namespace emb
