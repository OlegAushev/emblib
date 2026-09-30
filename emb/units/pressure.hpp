#pragma once

#include <emb/units/named_unit.hpp>

#include <concepts>

namespace emb {
namespace units {

namespace tags {

// Unit tag of the pascal.
struct pascal {};

// Unit tag of the megapascal.
struct megapascal {};

// Unit tag of the standard atmosphere.
struct atmosphere {};

} // namespace tags

// `pascal` is an alias template for a `named_unit` that holds a pressure in
// pascals.
template<std::floating_point T>
using pascal = named_unit<T, tags::pascal>;

// `megapascal` is an alias template for a `named_unit` that holds a pressure in
// megapascals.
template<std::floating_point T>
using megapascal = named_unit<T, tags::megapascal>;

// `atmosphere` is an alias template for a `named_unit` that holds a pressure in
// standard atmospheres.
template<std::floating_point T>
using atmosphere = named_unit<T, tags::atmosphere>;

// `pascal_f32` is `pascal` with a numerical value of type `float`.
using pascal_f32 = pascal<float>;

// `megapascal_f32` is `megapascal` with a numerical value of type `float`.
using megapascal_f32 = megapascal<float>;

// `atmosphere_f32` is `atmosphere` with a numerical value of type `float`.
using atmosphere_f32 = atmosphere<float>;

// pressure: 1 MPa = 1e6 Pa, 1 atm = 101325 Pa

// Converts `v` from pascals to megapascals.
template<typename To, std::floating_point T>
  requires std::same_as<To, megapascal<T>>
constexpr megapascal<T> convert_to(pascal<T> v)
{
  return units::megapascal<T>(v.value / T{1000000});
}

// Converts `v` from megapascals to pascals.
template<typename To, std::floating_point T>
  requires std::same_as<To, pascal<T>>
constexpr pascal<T> convert_to(megapascal<T> v)
{
  return units::pascal<T>(v.value * T{1000000});
}

// Converts `v` from pascals to standard atmospheres. One standard atmosphere
// is 101325 pascals.
template<typename To, std::floating_point T>
  requires std::same_as<To, atmosphere<T>>
constexpr atmosphere<T> convert_to(pascal<T> v)
{
  return units::atmosphere<T>(v.value / T{101325});
}

// Converts `v` from standard atmospheres to pascals. One standard atmosphere
// is 101325 pascals.
template<typename To, std::floating_point T>
  requires std::same_as<To, pascal<T>>
constexpr pascal<T> convert_to(atmosphere<T> v)
{
  return units::pascal<T>(v.value * T{101325});
}

// Converts `v` from megapascals to standard atmospheres. One standard
// atmosphere is 0.101325 megapascals.
template<typename To, std::floating_point T>
  requires std::same_as<To, atmosphere<T>>
constexpr atmosphere<T> convert_to(megapascal<T> v)
{
  return units::atmosphere<T>(v.value * T{1000000} / T{101325});
}

// Converts `v` from standard atmospheres to megapascals. One standard
// atmosphere is 0.101325 megapascals.
template<typename To, std::floating_point T>
  requires std::same_as<To, megapascal<T>>
constexpr megapascal<T> convert_to(atmosphere<T> v)
{
  return units::megapascal<T>(v.value * T{101325} / T{1000000});
}

} // namespace units
} // namespace emb
