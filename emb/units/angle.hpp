#pragma once

#include <emb/units/named_unit.hpp>

#include <emb/math.hpp>

#include <concepts>
#include <cstdint>

namespace emb {
namespace units {

namespace tags {

// Unit tag of the electrical radian.
struct erad {};

// Unit tag of the electrical degree.
struct edeg {};

// Unit tag of the radian.
struct rad {};

// Unit tag of the degree.
struct deg {};

} // namespace tags

// `erad` is an alias template for a `named_unit` that holds an electrical
// angle in electrical radians. The electrical angle of a machine is its
// mechanical angle times its number of pole pairs.
template<std::floating_point T>
using erad = named_unit<T, tags::erad>;

// `edeg` is an alias template for a `named_unit` that holds an electrical
// angle in electrical degrees.
template<std::floating_point T>
using edeg = named_unit<T, tags::edeg>;

// `rad` is an alias template for a `named_unit` that holds an angle in
// radians.
template<std::floating_point T>
using rad = named_unit<T, tags::rad>;

// `deg` is an alias template for a `named_unit` that holds an angle in
// degrees.
template<std::floating_point T>
using deg = named_unit<T, tags::deg>;

// `erad_f32` is `erad` with a numerical value of type `float`.
using erad_f32 = erad<float>;

// `edeg_f32` is `edeg` with a numerical value of type `float`.
using edeg_f32 = edeg<float>;

// `rad_f32` is `rad` with a numerical value of type `float`.
using rad_f32 = rad<float>;

// `deg_f32` is `deg` with a numerical value of type `float`.
using deg_f32 = deg<float>;

// The concept `unit_of_electrical_angle<T>` is satisfied if and only if `T`
// is a specialization of `erad` or `edeg`.
template<typename T>
concept unit_of_electrical_angle =
    some_unit<T>
    && (std::same_as<typename T::unit_type, tags::erad>
        || std::same_as<typename T::unit_type, tags::edeg>);

// The concept `unit_of_angle<T>` is satisfied if and only if `T` is a
// specialization of `rad` or `deg`; `erad` and `edeg` do not satisfy it.
template<typename T>
concept unit_of_angle = some_unit<T>
                     && (std::same_as<typename T::unit_type, tags::rad>
                         || std::same_as<typename T::unit_type, tags::deg>);

// The concept `unit_of_radians<T>` is satisfied if and only if `T` is a
// specialization of `erad` or `rad`.
template<typename T>
concept unit_of_radians = some_unit<T>
                       && (std::same_as<typename T::unit_type, tags::erad>
                           || std::same_as<typename T::unit_type, tags::rad>);

// The concept `unit_of_degrees<T>` is satisfied if and only if `T` is a
// specialization of `edeg` or `deg`.
template<typename T>
concept unit_of_degrees = some_unit<T>
                       && (std::same_as<typename T::unit_type, tags::edeg>
                           || std::same_as<typename T::unit_type, tags::deg>);

// Converts `v` from electrical degrees to electrical radians.
template<typename To, std::floating_point T>
  requires std::same_as<To, erad<T>>
constexpr erad<T> convert_to(edeg<T> v)
{
  return units::erad<T>(emb::to_rad(v.value));
}

// Converts `v` from electrical radians to electrical degrees.
template<typename To, std::floating_point T>
  requires std::same_as<To, edeg<T>>
constexpr edeg<T> convert_to(erad<T> v)
{
  return units::edeg<T>(emb::to_deg(v.value));
}

// Converts `v` from degrees to radians.
template<typename To, std::floating_point T>
  requires std::same_as<To, rad<T>>
constexpr rad<T> convert_to(deg<T> v)
{
  return units::rad<T>(emb::to_rad(v.value));
}

// Converts `v` from radians to degrees.
template<typename To, std::floating_point T>
  requires std::same_as<To, deg<T>>
constexpr deg<T> convert_to(rad<T> v)
{
  return units::deg<T>(emb::to_deg(v.value));
}

} // namespace units

// Normalizes the angle `v` into [0, 2pi), where 2pi stands for
// `2 * std::numbers::pi_v<typename Unit::value_type>`. The result is exact if
// `v.value >= 0` and correctly rounded otherwise, except that a result that
// rounds to 2pi becomes zero; a zero result is +0. If the numerical value of
// `v` is NaN or infinite, the result is NaN.
template<units::unit_of_radians Unit>
constexpr Unit norm2pi(Unit v)
{
  return Unit{norm2pi(v.value)};
}

// Normalizes the angle `v` into [-pi, pi), where pi stands for
// `std::numbers::pi_v<typename Unit::value_type>`. Even for `v` already in
// range the result is rounded: for a `float` numerical value and
// |`v`| <= 2pi, it is within 2.4e-7 of the exact value. If the numerical
// value of `v` is NaN or infinite, the result is NaN.
template<units::unit_of_radians Unit>
constexpr Unit normpi(Unit v)
{
  return Unit{normpi(v.value)};
}

// Approximates `norm2pi(v)`, returning an angle in [0, 2pi). For a `float`
// numerical value, the result differs from `norm2pi(v)`, modulo 2pi, by at
// most 4.8e-7 if |`v`| <= 2pi and by at most 7.6e-8 * |`v`| otherwise. The
// behavior is undefined if the numerical value of `v` is NaN or infinite or
// if |`v`| >= 1.3e10.
template<units::unit_of_radians Unit>
constexpr Unit norm2pi_fast(Unit v)
{
  return Unit{norm2pi_fast(v.value)};
}

// Approximates `normpi(v)`, returning an angle in [-pi, pi). For a `float`
// numerical value, the result differs from `normpi(v)`, modulo 2pi, by at
// most 4.8e-7 if |`v`| <= 2pi and by at most 1.1e-7 * |`v`| otherwise. The
// behavior is undefined if the numerical value of `v` is NaN or infinite or
// if |`v`| >= 1.3e10.
template<units::unit_of_radians Unit>
constexpr Unit normpi_fast(Unit v)
{
  return Unit{normpi_fast(v.value)};
}

// Normalizes the angle `v` into [0, 360). The result is exact if
// `v.value >= 0` and correctly rounded otherwise, except that a result that
// rounds to 360 becomes zero; a zero result is +0. If the numerical value of
// `v` is NaN or infinite, the result is NaN.
template<units::unit_of_degrees Unit>
constexpr Unit norm360(Unit v)
{
  using T = Unit::value_type;
  T val = emb::fmod(v.value, T{360});
  if (val <= 0) { // `fmod` returns -0 for the negative multiples of 360
    val += T{360};
    if (val >= T{360}) {
      // |val| was below half an ulp of 360, so the sum rounded onto the bound.
      val = T{0};
    }
  }
  return Unit{val};
}

// Normalizes the angle `v` into [-180, 180). Even for `v` already in range
// the result is rounded, and a result that rounds to 180 becomes -180: for a
// `float` numerical value and |`v`| <= 360, the result differs from the exact
// value, modulo 360, by at most 3.1e-5. If the numerical value of `v` is NaN
// or infinite, the result is NaN.
template<units::unit_of_degrees Unit>
constexpr Unit norm180(Unit v)
{
  using T = Unit::value_type;
  constexpr Unit offset{T{180}};
  return norm360(v + offset) - offset;
}

// Approximates `norm360(v)`, returning an angle in [0, 360). For a `float`
// numerical value, the result differs from `norm360(v)`, modulo 360, by at
// most 3.1e-5 if |`v`| <= 360 and by at most 8.5e-8 * |`v`| otherwise. The
// behavior is undefined if the numerical value of `v` is NaN or infinite or
// if |`v`| >= 7.7e11.
template<units::unit_of_degrees Unit>
constexpr Unit norm360_fast(Unit v)
{
  using T = Unit::value_type;
  constexpr T inv_360 = T{1} / T{360};
  T norm = v.value * inv_360;
  norm -= static_cast<T>(static_cast<std::int32_t>(norm) - (norm < T{0}));
  if (norm >= T{1}) norm = T{0};
  return Unit{norm * T{360}};
}

// Approximates `norm180(v)`, returning an angle in [-180, 180). For a `float`
// numerical value, the result differs from `norm180(v)`, modulo 360, by at
// most 3.1e-5 if |`v`| <= 360 and by at most 1.2e-7 * |`v`| otherwise. The
// behavior is undefined if the numerical value of `v` is NaN or infinite or
// if |`v`| >= 7.7e11.
template<units::unit_of_degrees Unit>
constexpr Unit norm180_fast(Unit v)
{
  using T = Unit::value_type;
  constexpr Unit offset{T{180}};
  return norm360_fast(v + offset) - offset;
}

} // namespace emb
