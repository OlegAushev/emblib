#pragma once

#include <emb/units/named_unit.hpp>

#include <concepts>

namespace emb {
namespace units {

namespace tags {

struct cubic_meter_per_hour {};
struct liter_per_minute {};

} // namespace tags

template<std::floating_point T>
using cubic_meter_per_hour = named_unit<T, tags::cubic_meter_per_hour>;

template<std::floating_point T>
using liter_per_minute = named_unit<T, tags::liter_per_minute>;

using cubic_meter_per_hour_f32 = cubic_meter_per_hour<float>;
using liter_per_minute_f32 = liter_per_minute<float>;

template<typename To, std::floating_point T>
  requires std::same_as<To, liter_per_minute<T>>
constexpr liter_per_minute<T> convert_to(cubic_meter_per_hour<T> v) {
  return units::liter_per_minute<T>(v.value() * T{1000} / T{60});
}

template<typename To, std::floating_point T>
  requires std::same_as<To, cubic_meter_per_hour<T>>
constexpr cubic_meter_per_hour<T> convert_to(liter_per_minute<T> v) {
  return units::cubic_meter_per_hour<T>(v.value() * T{60} / T{1000});
}

} // namespace units
} // namespace emb
