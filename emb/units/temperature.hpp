#pragma once

#include <emb/units/named_unit.hpp>

#include <concepts>

namespace emb {
namespace units {

namespace tags {

// Unit tag of the degree Celsius.
struct degree_celsius {};

// Unit tag of the kelvin.
struct kelvin {};

} // namespace tags

// `degree_celsius` is an alias template for a `named_unit` that holds a
// temperature in degrees Celsius.
template<std::floating_point T>
using degree_celsius = named_unit<T, tags::degree_celsius>;

// `kelvin` is an alias template for a `named_unit` that holds a temperature in
// kelvins.
template<std::floating_point T>
using kelvin = named_unit<T, tags::kelvin>;

// `degree_celsius_f32` is `degree_celsius` with a numerical value of type
// `float`.
using degree_celsius_f32 = degree_celsius<float>;

// `kelvin_f32` is `kelvin` with a numerical value of type `float`.
using kelvin_f32 = kelvin<float>;

// Converts `v` from degrees Celsius to kelvins. Adds 273.15 to the numerical
// value, which is correct for a temperature but not for a temperature
// difference.
template<typename To, std::floating_point T>
  requires std::same_as<To, kelvin<T>>
constexpr kelvin<T> convert_to(degree_celsius<T> v)
{
  return units::kelvin<T>(v.value + T{273.15});
}

// Converts `v` from kelvins to degrees Celsius. Subtracts 273.15 from the
// numerical value, which is correct for a temperature but not for a
// temperature difference.
template<typename To, std::floating_point T>
  requires std::same_as<To, degree_celsius<T>>
constexpr degree_celsius<T> convert_to(kelvin<T> v)
{
  return units::degree_celsius<T>(v.value - T{273.15});
}

} // namespace units
} // namespace emb
