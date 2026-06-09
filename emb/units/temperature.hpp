#pragma once

#include <emb/units/named_unit.hpp>

#include <concepts>

namespace emb {
namespace units {

namespace tags {

struct degree_celsius {};
struct kelvin {};

} // namespace tags

template<std::floating_point T>
using degree_celsius = named_unit<T, tags::degree_celsius>;

template<std::floating_point T>
using kelvin = named_unit<T, tags::kelvin>;

using degree_celsius_f32 = degree_celsius<float>;
using kelvin_f32 = kelvin<float>;

// temperature: T[K] = T[°C] + 273.15

template<typename To, std::floating_point T>
  requires std::same_as<To, kelvin<T>>
constexpr kelvin<T> convert_to(degree_celsius<T> v) {
  return units::kelvin<T>(v.value() + T{273.15});
}

template<typename To, std::floating_point T>
  requires std::same_as<To, degree_celsius<T>>
constexpr degree_celsius<T> convert_to(kelvin<T> v) {
  return units::degree_celsius<T>(v.value() - T{273.15});
}

} // namespace units
} // namespace emb
