#pragma once

#include <cmath>
#include <concepts>
#include <type_traits>

namespace emb {
namespace units {

// The class template `named_unit` holds a quantity as its numerical value, a
// number of type `T`, in the unit identified by `Unit`, e.g.
// `named_unit<float, tags::volt>` holds a voltage as a number of volts. `Unit`
// is the unit tag: a type that serves only to tell units apart.
//
// Specializations with different unit tags are distinct types, and there are
// no implicit conversions between a `named_unit` and `T`: an expression that
// mixes units, or passes a bare number where a `named_unit` is expected, does
// not compile. `convert_to` converts a `named_unit` to another unit.
template<std::floating_point T, typename Unit>
class named_unit {
public:
  using value_type = T;
  using unit_type = Unit;
public:
  value_type v_;
public:
  constexpr named_unit() : v_(value_type{0}) {}

  constexpr explicit named_unit(value_type v) : v_(v) {}

  constexpr value_type value() const
  {
    return v_;
  }

  constexpr named_unit& operator+=(named_unit rhs)
  {
    v_ += rhs.v_;
    return *this;
  }

  constexpr named_unit& operator-=(named_unit rhs)
  {
    v_ -= rhs.v_;
    return *this;
  }
};

template<std::floating_point T, typename Unit>
constexpr bool operator==(named_unit<T, Unit> lhs, named_unit<T, Unit> rhs)
{
  return lhs.value() == rhs.value();
}

template<std::floating_point T, typename Unit>
constexpr bool operator!=(named_unit<T, Unit> lhs, named_unit<T, Unit> rhs)
{
  return lhs.value() != rhs.value();
}

template<std::floating_point T, typename Unit>
constexpr bool operator<(named_unit<T, Unit> lhs, named_unit<T, Unit> rhs)
{
  return lhs.value() < rhs.value();
}

template<std::floating_point T, typename Unit>
constexpr bool operator>(named_unit<T, Unit> lhs, named_unit<T, Unit> rhs)
{
  return lhs.value() > rhs.value();
}

template<std::floating_point T, typename Unit>
constexpr bool operator<=(named_unit<T, Unit> lhs, named_unit<T, Unit> rhs)
{
  return lhs.value() <= rhs.value();
}

template<std::floating_point T, typename Unit>
constexpr bool operator>=(named_unit<T, Unit> lhs, named_unit<T, Unit> rhs)
{
  return lhs.value() >= rhs.value();
}

template<std::floating_point T, typename Unit>
constexpr named_unit<T, Unit> operator+(named_unit<T, Unit> lhs,
                                        named_unit<T, Unit> rhs)
{
  return lhs += rhs;
}

template<std::floating_point T, typename Unit>
constexpr named_unit<T, Unit> operator-(named_unit<T, Unit> lhs,
                                        named_unit<T, Unit> rhs)
{
  return lhs -= rhs;
}

template<std::floating_point T, typename Unit, typename V>
  requires std::is_arithmetic_v<V>
constexpr named_unit<T, Unit> operator*(named_unit<T, Unit> lhs, V rhs)
{
  return named_unit<T, Unit>(
      lhs.value() * static_cast<typename named_unit<T, Unit>::value_type>(rhs));
}

template<std::floating_point T, typename Unit, typename V>
  requires std::is_arithmetic_v<V>
constexpr named_unit<T, Unit> operator*(V lhs, named_unit<T, Unit> rhs)
{
  return rhs * lhs;
}

template<std::floating_point T, typename Unit, typename V>
  requires std::is_arithmetic_v<V>
constexpr named_unit<T, Unit> operator/(named_unit<T, Unit> lhs, V rhs)
{
  return named_unit<T, Unit>(
      lhs.value() / static_cast<typename named_unit<T, Unit>::value_type>(rhs));
}

template<std::floating_point T, typename Unit>
constexpr T operator/(named_unit<T, Unit> lhs, named_unit<T, Unit> rhs)
{
  return lhs.value() / rhs.value();
}

template<std::floating_point T, typename Unit>
constexpr named_unit<T, Unit> operator-(named_unit<T, Unit> v)
{
  return named_unit<T, Unit>(-v.value());
}

template<std::floating_point T, typename Unit>
constexpr named_unit<T, Unit> abs(named_unit<T, Unit> v)
{
  return named_unit<T, Unit>(std::abs(v.value()));
}

// The concept `some_unit<T>` is satisfied if and only if `T` is a
// specialization of `named_unit`. A cv-qualified or reference type does not
// satisfy it.
template<typename T>
concept some_unit =
    std::same_as<T, named_unit<typename T::value_type, typename T::unit_type>>;

// Converts `v` to its own type `To`, i.e. returns `v` unchanged. Ignores the
// remaining arguments, so generic code can pass the extra arguments that some
// other `convert_to` overloads take, e.g. the pole-pair count, without knowing
// whether `To` is the type of `v`.
template<typename To, typename From, typename... Args>
  requires std::same_as<To, From>
constexpr To convert_to(From v, Args...)
{
  return v;
}

} // namespace units
} // namespace emb
