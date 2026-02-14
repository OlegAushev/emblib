#pragma once

#include <emb/foc/pipe.hpp>
#include <emb/motorcontrol.hpp>

namespace emb {
namespace foc {

class park {
  float sine_;
  float cosine_;
public:
  constexpr park(float sine, float cosine) : sine_(sine), cosine_(cosine) {}

  constexpr vec_dq operator()(vec_alphabeta const& ab) const {
    return park_transform(ab, sine_, cosine_);
  }
};

class invpark {
  float sine_;
  float cosine_;
public:
  constexpr invpark(float sine, float cosine) : sine_(sine), cosine_(cosine) {}

  constexpr vec_alphabeta operator()(vec_dq const& dq) const {
    return invpark_transform(dq, sine_, cosine_);
  }
};

} // namespace foc
} // namespace emb
