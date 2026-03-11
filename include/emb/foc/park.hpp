#pragma once

#include <emb/foc/types.hpp>

namespace emb {
namespace foc {

constexpr vec_dq park_transform(vec_ab v_ab, float sine, float cosine) {
  return {
      .d = (v_ab.alpha * cosine) + (v_ab.beta * sine),
      .q = (v_ab.beta * cosine) - (v_ab.alpha * sine)
  };
}

constexpr vec_ab invpark_transform(vec_dq v_dq, float sine, float cosine) {
  return {
      .alpha = (v_dq.d * cosine) - (v_dq.q * sine),
      .beta = (v_dq.q * cosine) + (v_dq.d * sine)
  };
}

class park {
private:
  float sine_;
  float cosine_;
public:
  constexpr park(float sine, float cosine) : sine_(sine), cosine_(cosine) {}

  constexpr vec_dq operator()(vec_ab v_ab) const {
    return park_transform(v_ab, sine_, cosine_);
  }
};

class invpark {
private:
  float sine_;
  float cosine_;
public:
  constexpr invpark(float sine, float cosine) : sine_(sine), cosine_(cosine) {}

  constexpr vec_ab operator()(vec_dq v_dq) const {
    return invpark_transform(v_dq, sine_, cosine_);
  }
};

} // namespace foc
} // namespace emb
