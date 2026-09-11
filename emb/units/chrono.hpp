#pragma once

#include <emb/chrono.hpp>
#include <emb/units/named_unit.hpp>

#include <chrono>
#include <concepts>
#include <type_traits>

namespace emb {
namespace units {

namespace tags {

struct hz {};
struct sec {};

} // namespace tags

template<std::floating_point T>
using hz = named_unit<T, tags::hz>;

template<std::floating_point T>
using sec = named_unit<T, tags::sec>;

using hz_f32 = hz<float>;
using sec_f32 = sec<float>;

template<std::floating_point T, typename V>
  requires std::is_arithmetic_v<V>
constexpr sec<T> operator/(V lhs, hz<T> rhs)
{
  return sec<T>(static_cast<T>(lhs) / rhs.value());
}

template<std::floating_point T, typename V>
  requires std::is_arithmetic_v<V>
constexpr hz<T> operator/(V lhs, sec<T> rhs)
{
  return hz<T>(static_cast<T>(lhs) / rhs.value());
}

template<emb::chrono::some_duration Duration, std::floating_point T>
constexpr Duration to_duration(sec<T> t)
{
  return std::chrono::duration_cast<Duration>(
      std::chrono::duration<T>{t.value()});
}

template<std::floating_point T>
constexpr std::chrono::milliseconds to_milliseconds(sec<T> t)
{
  return to_duration<std::chrono::milliseconds>(t);
}

} // namespace units
} // namespace emb
