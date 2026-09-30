#pragma once

#include <emb/units/named_unit.hpp>

#include <concepts>

namespace emb {
namespace units {

namespace tags {

// Unit tag of the cubic meter per hour.
struct cubic_meter_per_hour {};

// Unit tag of the liter per minute.
struct liter_per_minute {};

} // namespace tags

// `cubic_meter_per_hour` is an alias template for a `named_unit` that holds a
// volumetric flow rate in cubic meters per hour.
template<std::floating_point T>
using cubic_meter_per_hour = named_unit<T, tags::cubic_meter_per_hour>;

// `liter_per_minute` is an alias template for a `named_unit` that holds a
// volumetric flow rate in liters per minute.
template<std::floating_point T>
using liter_per_minute = named_unit<T, tags::liter_per_minute>;

// `cubic_meter_per_hour_f32` is `cubic_meter_per_hour` with a numerical value
// of type `float`.
using cubic_meter_per_hour_f32 = cubic_meter_per_hour<float>;

// `liter_per_minute_f32` is `liter_per_minute` with a numerical value of type
// `float`.
using liter_per_minute_f32 = liter_per_minute<float>;

// Converts `v` from cubic meters per hour to liters per minute. One liter per
// minute is 0.06 cubic meters per hour.
template<typename To, std::floating_point T>
  requires std::same_as<To, liter_per_minute<T>>
constexpr liter_per_minute<T> convert_to(cubic_meter_per_hour<T> v)
{
  return units::liter_per_minute<T>(v.value * T{1000} / T{60});
}

// Converts `v` from liters per minute to cubic meters per hour. One liter per
// minute is 0.06 cubic meters per hour.
template<typename To, std::floating_point T>
  requires std::same_as<To, cubic_meter_per_hour<T>>
constexpr cubic_meter_per_hour<T> convert_to(liter_per_minute<T> v)
{
  return units::cubic_meter_per_hour<T>(v.value * T{60} / T{1000});
}

} // namespace units
} // namespace emb
