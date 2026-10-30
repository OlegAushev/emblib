#pragma once

#include <emb/i18n.hpp>

#include <cmath>
#include <compare>
#include <concepts>
#include <type_traits>

namespace emb {
namespace units {

// The class template `quantity` holds a physical quantity as its numerical
// value `value`, a number of type `T`, in the unit identified by `Unit`, e.g.
// `quantity<float, tags::volt>` holds a voltage as a number of volts. `Unit`
// is the unit tag: a type that serves only to tell units apart. `quantity` is
// a structural type, so a `quantity`, or an object whose members are
// `quantity`s, can be the argument of a non-type template parameter, as in
// `template<auto stage>`.
//
// Specializations with different unit tags are distinct types, and there are
// no implicit conversions between a `quantity` and `T`: an expression that
// mixes units, or passes a bare number where a `quantity` is expected, does
// not compile. `convert_to` converts a `quantity` to another unit.
template<std::floating_point T, typename Unit>
struct quantity {
  using value_type = T;
  using unit_type = Unit;

  value_type value;

  constexpr quantity() : value(value_type{0}) {}

  constexpr explicit quantity(value_type v) : value(v) {}

  constexpr quantity& operator+=(quantity rhs)
  {
    value += rhs.value;
    return *this;
  }

  constexpr quantity& operator-=(quantity rhs)
  {
    value -= rhs.value;
    return *this;
  }

  // Returns the numerical value of `v`, i.e. `v.value`. Generic code that
  // handles any type wrapping a single number, e.g. `emb::settings`, calls
  // `value_of` unqualified: as a friend defined in the class, it is found
  // only by argument-dependent lookup.
  friend constexpr value_type value_of(quantity v)
  {
    return v.value;
  }

  // Returns the symbol of the unit in each language, the `symbol` of the unit
  // tag, e.g. "V" and "В" for `tags::volt`; declared only if the tag has one.
  // Generic code asks for it through `emb::unit_symbol_of<T>()`, which calls
  // it unqualified: as a friend defined in the class, it is found only by
  // argument-dependent lookup.
  friend constexpr i18n::text unit_symbol_of(quantity)
    requires requires {
      { Unit::symbol } -> std::convertible_to<i18n::text>;
    }
  {
    return Unit::symbol;
  }

  friend constexpr auto operator<=>(quantity, quantity) = default;

  friend constexpr quantity operator+(quantity lhs, quantity rhs)
  {
    return lhs += rhs;
  }

  friend constexpr quantity operator-(quantity lhs, quantity rhs)
  {
    return lhs -= rhs;
  }

  template<typename V>
    requires std::is_arithmetic_v<V>
  friend constexpr quantity operator*(quantity lhs, V rhs)
  {
    return quantity{lhs.value * static_cast<value_type>(rhs)};
  }

  template<typename V>
    requires std::is_arithmetic_v<V>
  friend constexpr quantity operator*(V lhs, quantity rhs)
  {
    return rhs * lhs;
  }

  template<typename V>
    requires std::is_arithmetic_v<V>
  friend constexpr quantity operator/(quantity lhs, V rhs)
  {
    return quantity{lhs.value / static_cast<value_type>(rhs)};
  }

  friend constexpr value_type operator/(quantity lhs, quantity rhs)
  {
    return lhs.value / rhs.value;
  }

  friend constexpr quantity operator-(quantity v)
  {
    return quantity{-v.value};
  }

  friend constexpr quantity abs(quantity v)
  {
    return quantity{std::abs(v.value)};
  }
};

// The concept `some_quantity<T>` is satisfied if and only if `T` is a
// specialization of `quantity`. A cv-qualified or reference type does not
// satisfy it.
template<typename T>
concept some_quantity =
    std::same_as<T, quantity<typename T::value_type, typename T::unit_type>>;

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
