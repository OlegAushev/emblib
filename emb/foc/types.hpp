#pragma once

#include <emb/math.hpp>
#include <emb/three_phase.hpp>
#include <emb/units/angle.hpp>
#include <emb/units/electrical.hpp>

namespace emb {
namespace foc {

// Space vectors are tagged with the quantity they carry (emb::units::tags),
// so currents and voltages are distinct types at every interface while the
// components stay plain float for the arithmetic inside the stages.

template<typename Q>
struct vec_abc {
  float a;
  float b;
  float c;
};

template<typename Q>
struct vec_polar {
  float mag;
  float theta;
};

template<typename Q>
struct vec_ab {
  float alpha;
  float beta;
};

template<typename Q>
struct vec_dq {
  float d;
  float q;
};

using current_abc = vec_abc<units::tags::amp>;
using current_ab = vec_ab<units::tags::amp>;
using current_dq = vec_dq<units::tags::amp>;
using voltage_abc = vec_abc<units::tags::volt>;
using voltage_ab = vec_ab<units::tags::volt>;
using voltage_dq = vec_dq<units::tags::volt>;
using voltage_polar = vec_polar<units::tags::volt>;

// The struct `sin_cos` holds an angle theta as its sine and cosine, the form
// the Park transforms take: (`cosine`, `sine`) is the unit vector of the d axis
// in the stationary alpha-beta frame. A default-constructed `sin_cos` holds the
// angle zero.
struct sin_cos {
  float sine = 0.0f;
  float cosine = 1.0f;
};

// Returns the sine and cosine of `theta`, computed with `emb::sin` and
// `emb::cos`.
constexpr sin_cos to_sin_cos(units::erad_f32 theta)
{
  return {.sine = emb::sin(theta.value), .cosine = emb::cos(theta.value)};
}

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
