#pragma once

#include <cmath>
#include <compare>
#include <concepts>
#include <type_traits>

namespace emb {
namespace units {

// The class template `named_unit` holds a quantity as its numerical value
// `value`, a number of type `T`, in the unit identified by `Unit`, e.g.
// `named_unit<float, tags::volt>` holds a voltage as a number of volts. `Unit`
// is the unit tag: a type that serves only to tell units apart. `named_unit`
// is a structural type, so a `named_unit`, or an object whose members are
// `named_unit`s, can be the argument of a non-type template parameter, as in
// `template<auto stage>`.
//
// Specializations with different unit tags are distinct types, and there are
// no implicit conversions between a `named_unit` and `T`: an expression that
// mixes units, or passes a bare number where a `named_unit` is expected, does
// not compile. `convert_to` converts a `named_unit` to another unit.
template<std::floating_point T, typename Unit>
struct named_unit {
  using value_type = T;
  using unit_type = Unit;

  value_type value;

  constexpr named_unit() : value(value_type{0}) {}

  constexpr explicit named_unit(value_type v) : value(v) {}

  constexpr named_unit& operator+=(named_unit rhs)
  {
    value += rhs.value;
    return *this;
  }

  constexpr named_unit& operator-=(named_unit rhs)
  {
    value -= rhs.value;
    return *this;
  }

  // Returns the numerical value of `v`, i.e. `v.value`. Generic code that
  // handles any type wrapping a single number, e.g. `emb::settings`, calls
  // `value_of` unqualified: as a friend defined in the class, it is found
  // only by argument-dependent lookup.
  friend constexpr value_type value_of(named_unit v)
  {
    return v.value;
  }

  friend constexpr auto operator<=>(named_unit, named_unit) = default;

  friend constexpr named_unit operator+(named_unit lhs, named_unit rhs)
  {
    return lhs += rhs;
  }

  friend constexpr named_unit operator-(named_unit lhs, named_unit rhs)
  {
    return lhs -= rhs;
  }

  template<typename V>
    requires std::is_arithmetic_v<V>
  friend constexpr named_unit operator*(named_unit lhs, V rhs)
  {
    return named_unit{lhs.value * static_cast<value_type>(rhs)};
  }

  template<typename V>
    requires std::is_arithmetic_v<V>
  friend constexpr named_unit operator*(V lhs, named_unit rhs)
  {
    return rhs * lhs;
  }

  template<typename V>
    requires std::is_arithmetic_v<V>
  friend constexpr named_unit operator/(named_unit lhs, V rhs)
  {
    return named_unit{lhs.value / static_cast<value_type>(rhs)};
  }

  friend constexpr value_type operator/(named_unit lhs, named_unit rhs)
  {
    return lhs.value / rhs.value;
  }

  friend constexpr named_unit operator-(named_unit v)
  {
    return named_unit{-v.value};
  }

  friend constexpr named_unit abs(named_unit v)
  {
    return named_unit{std::abs(v.value)};
  }
};

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
