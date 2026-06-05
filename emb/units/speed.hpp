#pragma once

#include <emb/units/named_unit.hpp>

#include <emb/math.hpp>

#include <concepts>

namespace emb {
namespace units {

namespace tags {

struct rpm {};
struct eradps {};

} // namespace tags

template<std::floating_point T>
using rpm = named_unit<T, tags::rpm>;

template<std::floating_point T>
using eradps = named_unit<T, tags::eradps>;

using rpm_f32 = rpm<float>;
using eradps_f32 = eradps<float>;

template<typename T>
concept unit_of_rotational_speed =
    unit<T> && (std::same_as<typename T::unit_type, tags::rpm> ||
                std::same_as<typename T::unit_type, tags::eradps>);

template<typename To, std::floating_point T, std::integral P>
  requires std::same_as<To, eradps<T>>
constexpr eradps<T> convert_to(rpm<T> v, P p) {
  return units::eradps<T>(emb::to_eradps(v.value(), p));
}

template<typename To, std::floating_point T, std::integral P>
  requires std::same_as<To, rpm<T>>
constexpr rpm<T> convert_to(eradps<T> v, P p) {
  return units::rpm<T>(emb::to_rpm(v.value(), p));
}

} // namespace units
} // namespace emb
