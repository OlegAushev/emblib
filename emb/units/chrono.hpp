#pragma once

#include <emb/chrono.hpp>
#include <emb/units/named_unit.hpp>

#include <chrono>
#include <concepts>
#include <type_traits>

namespace emb {
namespace units {

namespace tags {

// Unit tag of the hertz.
struct hz {};

// Unit tag of the second.
struct sec {};

} // namespace tags

// `hz` is an alias template for a `named_unit` that holds a frequency in hertz.
template<std::floating_point T>
using hz = named_unit<T, tags::hz>;

// `sec` is an alias template for a `named_unit` that holds a time interval in
// seconds.
template<std::floating_point T>
using sec = named_unit<T, tags::sec>;

// `hz_f32` is `hz` with a numerical value of type `float`.
using hz_f32 = hz<float>;

// `sec_f32` is `sec` with a numerical value of type `float`.
using sec_f32 = sec<float>;

// Returns `lhs`, which is converted to `T` first, divided by the frequency
// `rhs`, as a time interval in seconds, e.g. `1 / rhs` is the period of `rhs`.
template<std::floating_point T, typename V>
  requires std::is_arithmetic_v<V>
constexpr sec<T> operator/(V lhs, hz<T> rhs)
{
  return sec<T>(static_cast<T>(lhs) / rhs.value());
}

// Returns `lhs`, which is converted to `T` first, divided by the time interval
// `rhs`, as a frequency in hertz, e.g. `1 / rhs` is the frequency whose period
// is `rhs`.
template<std::floating_point T, typename V>
  requires std::is_arithmetic_v<V>
constexpr hz<T> operator/(V lhs, sec<T> rhs)
{
  return hz<T>(static_cast<T>(lhs) / rhs.value());
}

// Converts the time interval `t` to `Duration`, a specialization of
// `std::chrono::duration`, as if by `std::chrono::duration_cast` from a
// duration of `t.value()` seconds with representation `T`. If `Duration::rep`
// is an integer type, the number of ticks is computed in `T` and truncated
// toward zero, and the behavior is undefined if `t` is NaN or infinite or the
// number of ticks is not representable by `Duration::rep`.
template<emb::chrono::some_duration Duration, std::floating_point T>
constexpr Duration to_duration(sec<T> t)
{
  return std::chrono::duration_cast<Duration>(
      std::chrono::duration<T>{t.value()});
}

// Converts the time interval `t` to milliseconds, i.e.
// `to_duration<std::chrono::milliseconds>(t)`. The number of milliseconds,
// `t.value() * 1000` computed in `T`, is truncated toward zero, e.g. 1.5 ms
// becomes 1 ms and -1.5 ms becomes -1 ms. The behavior is undefined if `t` is
// NaN or infinite or the number of milliseconds is not representable by
// `std::chrono::milliseconds::rep`.
template<std::floating_point T>
constexpr std::chrono::milliseconds to_milliseconds(sec<T> t)
{
  return to_duration<std::chrono::milliseconds>(t);
}

} // namespace units
} // namespace emb
