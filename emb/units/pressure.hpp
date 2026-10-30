#pragma once

#include <emb/units/quantity.hpp>

#include <emb/i18n.hpp>

#include <concepts>

namespace emb {
namespace units {

namespace tags {

// Unit tag of the pascal, whose symbol is "Pa", in Russian "Па".
struct pascal {
  static constexpr i18n::text symbol = {"Pa", "Па"};
};

// Unit tag of the megapascal, whose symbol is "MPa", in Russian "МПа".
struct megapascal {
  static constexpr i18n::text symbol = {"MPa", "МПа"};
};

// Unit tag of the standard atmosphere, whose symbol is "atm", in Russian
// "атм".
struct atmosphere {
  static constexpr i18n::text symbol = {"atm", "атм"};
};

} // namespace tags

// `pascal` is an alias template for a `quantity` that holds a pressure in
// pascals.
template<std::floating_point T>
using pascal = quantity<T, tags::pascal>;

// `megapascal` is an alias template for a `quantity` that holds a pressure in
// megapascals.
template<std::floating_point T>
using megapascal = quantity<T, tags::megapascal>;

// `atmosphere` is an alias template for a `quantity` that holds a pressure in
// standard atmospheres.
template<std::floating_point T>
using atmosphere = quantity<T, tags::atmosphere>;

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
