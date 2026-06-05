#pragma once

#include <emb/units/named_unit.hpp>

#include <concepts>

namespace emb {
namespace units {

namespace tags {

struct pascal {};
struct megapascal {};
struct atmosphere {};

} // namespace tags

template<std::floating_point T>
using pascal = named_unit<T, tags::pascal>;

template<std::floating_point T>
using megapascal = named_unit<T, tags::megapascal>;

template<std::floating_point T>
using atmosphere = named_unit<T, tags::atmosphere>;

using pascal_f32 = pascal<float>;
using megapascal_f32 = megapascal<float>;
using atmosphere_f32 = atmosphere<float>;

// pressure: 1 MPa = 1e6 Pa, 1 atm = 101325 Pa

template<typename To, std::floating_point T>
  requires std::same_as<To, megapascal<T>>
constexpr megapascal<T> convert_to(pascal<T> v) {
  return units::megapascal<T>(v.value() / T{1000000});
}

template<typename To, std::floating_point T>
  requires std::same_as<To, pascal<T>>
constexpr pascal<T> convert_to(megapascal<T> v) {
  return units::pascal<T>(v.value() * T{1000000});
}

template<typename To, std::floating_point T>
  requires std::same_as<To, atmosphere<T>>
constexpr atmosphere<T> convert_to(pascal<T> v) {
  return units::atmosphere<T>(v.value() / T{101325});
}

template<typename To, std::floating_point T>
  requires std::same_as<To, pascal<T>>
constexpr pascal<T> convert_to(atmosphere<T> v) {
  return units::pascal<T>(v.value() * T{101325});
}

template<typename To, std::floating_point T>
  requires std::same_as<To, atmosphere<T>>
constexpr atmosphere<T> convert_to(megapascal<T> v) {
  return units::atmosphere<T>(v.value() * T{1000000} / T{101325});
}

template<typename To, std::floating_point T>
  requires std::same_as<To, megapascal<T>>
constexpr megapascal<T> convert_to(atmosphere<T> v) {
  return units::megapascal<T>(v.value() * T{101325} / T{1000000});
}

} // namespace units
} // namespace emb
