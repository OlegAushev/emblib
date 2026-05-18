#pragma once

#include <emb/math.hpp>

#include <array>

namespace emb {
namespace foc {

struct vec_polar {
  float mag;
  float theta;
};

struct vec_ab {
  float alpha;
  float beta;
};

struct vec_dq {
  float d;
  float q;
};

template<typename T>
concept some_motor = requires(T m) {
  { m.p } -> std::convertible_to<int>;
  { m.R } -> std::convertible_to<float>;
  { m.Ld } -> std::convertible_to<float>;
  { m.Lq } -> std::convertible_to<float>;
  { m.Psi } -> std::convertible_to<float>;
};

} // namespace foc
} // namespace emb
