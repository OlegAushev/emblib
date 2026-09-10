#pragma once

#include <emb/foc/types.hpp>
#include <emb/pipe.hpp>

namespace emb {
namespace foc {

class park_transform : public pipe::pipeable<park_transform> {
  float sine_;
  float cosine_;
public:
  constexpr park_transform(float sine, float cosine)
      : sine_{sine}, cosine_{cosine}
  {
  }

  template<typename Q>
  constexpr vec_dq<Q> operator()(vec_ab<Q> v_ab) const
  {
    return {.d = (v_ab.alpha * cosine_) + (v_ab.beta * sine_),
            .q = (v_ab.beta * cosine_) - (v_ab.alpha * sine_)};
  }
};

class invpark_transform : public pipe::pipeable<invpark_transform> {
  float sine_;
  float cosine_;
public:
  constexpr invpark_transform(float sine, float cosine)
      : sine_{sine}, cosine_{cosine}
  {
  }

  template<typename Q>
  constexpr vec_ab<Q> operator()(vec_dq<Q> v_dq) const
  {
    return {.alpha = (v_dq.d * cosine_) - (v_dq.q * sine_),
            .beta = (v_dq.q * cosine_) + (v_dq.d * sine_)};
  }
};

} // namespace foc
} // namespace emb
