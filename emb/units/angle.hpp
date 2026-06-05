#pragma once

#include <emb/units/named_unit.hpp>

#include <emb/math.hpp>

#include <concepts>
#include <cstdint>

namespace emb {
namespace units {

namespace tags {

struct erad {};
struct edeg {};
struct rad {};
struct deg {};

} // namespace tags

template<std::floating_point T>
using erad = named_unit<T, tags::erad>;

template<std::floating_point T>
using edeg = named_unit<T, tags::edeg>;

template<std::floating_point T>
using rad = named_unit<T, tags::rad>;

template<std::floating_point T>
using deg = named_unit<T, tags::deg>;

using erad_f32 = erad<float>;
using edeg_f32 = edeg<float>;
using rad_f32 = rad<float>;
using deg_f32 = deg<float>;

template<typename T>
concept unit_of_electrical_angle =
    unit<T> && (std::same_as<typename T::unit_type, tags::erad> ||
                std::same_as<typename T::unit_type, tags::edeg>);

template<typename T>
concept unit_of_angle = unit<T> &&
                        (std::same_as<typename T::unit_type, tags::rad> ||
                         std::same_as<typename T::unit_type, tags::deg>);

template<typename T>
concept unit_of_radians = unit<T> &&
                          (std::same_as<typename T::unit_type, tags::erad> ||
                           std::same_as<typename T::unit_type, tags::rad>);

template<typename T>
concept unit_of_degrees = unit<T> &&
                          (std::same_as<typename T::unit_type, tags::edeg> ||
                           std::same_as<typename T::unit_type, tags::deg>);

template<typename To, std::floating_point T>
  requires std::same_as<To, erad<T>>
constexpr erad<T> convert_to(edeg<T> v) {
  return units::erad<T>(emb::to_rad(v.value()));
}

template<typename To, std::floating_point T>
  requires std::same_as<To, edeg<T>>
constexpr edeg<T> convert_to(erad<T> v) {
  return units::edeg<T>(emb::to_deg(v.value()));
}

template<typename To, std::floating_point T>
  requires std::same_as<To, rad<T>>
constexpr rad<T> convert_to(deg<T> v) {
  return units::rad<T>(emb::to_rad(v.value()));
}

template<typename To, std::floating_point T>
  requires std::same_as<To, deg<T>>
constexpr deg<T> convert_to(rad<T> v) {
  return units::deg<T>(emb::to_deg(v.value()));
}

} // namespace units

template<units::unit_of_radians Unit>
constexpr Unit norm2pi(Unit v) {
  return Unit{norm2pi(v.value())};
}

template<units::unit_of_radians Unit>
constexpr Unit normpi(Unit v) {
  return Unit{normpi(v.value())};
}

template<units::unit_of_radians Unit>
constexpr Unit norm2pi_fast(Unit v) {
  return Unit{norm2pi_fast(v.value())};
}

template<units::unit_of_radians Unit>
constexpr Unit normpi_fast(Unit v) {
  return Unit{normpi_fast(v.value())};
}

template<units::unit_of_degrees Unit>
constexpr Unit norm360(Unit v) {
  using T = Unit::value_type;
  T val = emb::fmod(v.value(), T{360});
  if (val < 0) {
    val += T{360};
  }
  return Unit{val};
}

template<units::unit_of_degrees Unit>
constexpr Unit norm180(Unit v) {
  using T = Unit::value_type;
  constexpr Unit offset{T{180}};
  return norm360(v + offset) - offset;
}

template<units::unit_of_degrees Unit>
constexpr Unit norm360_fast(Unit v) {
  using T = Unit::value_type;
  constexpr T inv_360 = T{1} / T{360};
  T norm = v.value() * inv_360;
  norm -= static_cast<T>(static_cast<std::int32_t>(norm) - (norm < T{0}));
  if (norm >= T{1}) norm -= T{1};
  return Unit{norm * T{360}};
}

template<units::unit_of_degrees Unit>
constexpr Unit norm180_fast(Unit v) {
  using T = Unit::value_type;
  constexpr Unit offset{T{180}};
  return norm360_fast(v + offset) - offset;
}

} // namespace emb
