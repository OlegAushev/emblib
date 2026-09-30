#pragma once

#include <emb/units/named_unit.hpp>

#include <emb/math.hpp>

#include <concepts>

namespace emb {
namespace units {

namespace tags {

// Unit tag of the revolution per minute.
struct rpm {};

// Unit tag of the electrical radian per second.
struct eradps {};

} // namespace tags

// `rpm` is an alias template for a `named_unit` that holds a mechanical
// rotational speed in revolutions per minute.
template<std::floating_point T>
using rpm = named_unit<T, tags::rpm>;

// `eradps` is an alias template for a `named_unit` that holds an electrical
// angular speed in electrical radians per second.
template<std::floating_point T>
using eradps = named_unit<T, tags::eradps>;

// `rpm_f32` is `rpm` with a numerical value of type `float`.
using rpm_f32 = rpm<float>;

// `eradps_f32` is `eradps` with a numerical value of type `float`.
using eradps_f32 = eradps<float>;

// The concept `unit_of_rotational_speed<T>` is satisfied if and only if `T` is
// a `named_unit` that holds a rotational speed, i.e. whose unit tag is
// `tags::rpm` or `tags::eradps`.
template<typename T>
concept unit_of_rotational_speed =
    some_unit<T>
    && (std::same_as<typename T::unit_type, tags::rpm>
        || std::same_as<typename T::unit_type, tags::eradps>);

// Converts `v` from revolutions per minute to electrical radians per second.
// `v` is the mechanical speed of a machine with `p` pole pairs, and one
// revolution per minute is 2pi * `p` / 60 electrical radians per second.
template<typename To, std::floating_point T, std::integral P>
  requires std::same_as<To, eradps<T>>
constexpr eradps<T> convert_to(rpm<T> v, P p)
{
  return units::eradps<T>(emb::to_eradps(v.value, p));
}

// Converts `v` from electrical radians per second to revolutions per minute.
// `v` is the electrical angular speed of a machine with `p` pole pairs, and
// one revolution per minute is 2pi * `p` / 60 electrical radians per second.
// The behavior is undefined if `p` is zero.
template<typename To, std::floating_point T, std::integral P>
  requires std::same_as<To, rpm<T>>
constexpr rpm<T> convert_to(eradps<T> v, P p)
{
  return units::rpm<T>(emb::to_rpm(v.value, p));
}

} // namespace units
} // namespace emb
