#pragma once

#include <emb/foc/types.hpp>

namespace emb {
namespace foc {

constexpr vec_dq park_transform(vec_ab v, float sine, float cosine) {
  vec_dq retv;
  retv.d = (v.alpha * cosine) + (v.beta * sine);
  retv.q = (v.beta * cosine) - (v.alpha * sine);
  return retv;
}

constexpr vec_ab invpark_transform(vec_dq v, float sine, float cosine) {
  vec_ab retv;
  retv.alpha = (v.d * cosine) - (v.q * sine);
  retv.beta = (v.q * cosine) + (v.d * sine);
  return retv;
}

class park {
  float sine_;
  float cosine_;
public:
  constexpr park(float sine, float cosine) : sine_(sine), cosine_(cosine) {}

  constexpr vec_dq operator()(vec_ab const& ab) const {
    return park_transform(ab, sine_, cosine_);
  }
};

class invpark {
  float sine_;
  float cosine_;
public:
  constexpr invpark(float sine, float cosine) : sine_(sine), cosine_(cosine) {}

  constexpr vec_ab operator()(vec_dq const& dq) const {
    return invpark_transform(dq, sine_, cosine_);
  }
};

} // namespace foc
} // namespace emb
