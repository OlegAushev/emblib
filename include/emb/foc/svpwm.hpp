#pragma once

#include <emb/foc/pipe.hpp>
#include <emb/motorcontrol.hpp>

#include <array>

namespace emb {
namespace foc {

class svpwm {
  float vDC_;
public:
  constexpr svpwm(float vDC) : vDC_(vDC) {}

  std::array<unsigned_pu, 3> operator()(vec_alpha v) const {
    return calculate_svpwm(v, vDC_);
  }
};

} // namespace foc
} // namespace emb
