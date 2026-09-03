#pragma once

#include <emb/meta/alternative_of.hpp>

#include <bit>
#include <concepts>
#include <optional>
#include <type_traits>
#include <variant>

#include <cstdint>

namespace emb {
namespace settings {

// The closed set of scalars a parameter may hold, and the type-erased value
// transports and diagnostics see. Narrow integers buy nothing in a four-byte
// cell, and a parameter that needs one is better modelled as int32 than as
// a fifth code path through every layer. Adding one later widens a concept
// and breaks nothing.
enum class value_type : std::uint8_t {
  boolean,
  int32,
  uint32,
  float32,
};

using value = std::variant<bool, std::int32_t, std::uint32_t, float>;

// One storage cell. Every value occupies four bytes whatever its type: the
// largest scalar, the expedited-SDO payload, and the natural word of both
// the RAM image and the NVM record. Uniformity is worth the padding — a
// bool costs three bytes and buys an image that is a plain array and a
// record whose cells need no per-entry size.
using raw_value = std::uint32_t;

template<typename T>
concept some_value = alternative_of<T, value>;

// value_type's codes are the variant's alternative indices; held_type()
// below is a cast, and the two cannot silently drift apart.
static_assert(std::variant_size_v<value> == 4);
static_assert(std::same_as<std::variant_alternative_t<0, value>, bool>);
static_assert(std::same_as<std::variant_alternative_t<1, value>, std::int32_t>);
static_assert(
    std::same_as<std::variant_alternative_t<2, value>, std::uint32_t>);
static_assert(std::same_as<std::variant_alternative_t<3, value>, float>);

// A type that wraps one scalar and can be rebuilt from it: emb::units::
// named_unit, emb::clamped, and anything else shaped like them. Recognised
// structurally rather than by name, so this header sits at the bottom of
// the dependency graph and knows about neither.
//
// Only the wrapper's scalar is ever stored, so its layout is nobody's
// business here: conversion goes through value() and the constructor, never
// through the object's bytes.
template<typename T>
concept some_wrapped_value = requires { typename T::value_type; }
                          && some_value<typename T::value_type>
                          && std::constructible_from<T, typename T::value_type>
                          && requires(T const& v) {
                               {
                                 v.value()
                               } -> std::same_as<typename T::value_type>;
                             };

template<typename T>
concept some_parameter_type = some_value<T> || some_wrapped_value<T>;

namespace detail {

template<typename T>
struct scalar_of {
  using type = T;
};

template<some_wrapped_value T>
struct scalar_of<T> {
  using type = typename T::value_type;
};

template<some_value T>
consteval value_type tag_of()
{
  if constexpr (std::same_as<T, bool>) {
    return value_type::boolean;
  }
  else if constexpr (std::same_as<T, std::int32_t>) {
    return value_type::int32;
  }
  else if constexpr (std::same_as<T, std::uint32_t>) {
    return value_type::uint32;
  }
  else {
    return value_type::float32;
  }
}

} // namespace detail

// The scalar a parameter type is stored and transported as: itself, or the
// thing its wrapper is made of.
template<some_parameter_type T>
using scalar_t = typename detail::scalar_of<T>::type;

template<some_parameter_type T>
inline constexpr value_type type_of = detail::tag_of<scalar_t<T>>();

constexpr auto held_type(value const& v) -> value_type
{
  return static_cast<value_type>(v.index());
}

// -- Typed value <-> type-erased value --

template<some_parameter_type T>
constexpr auto to_value(T const& v) -> value
{
  if constexpr (some_value<T>) {
    return value{v};
  }
  else {
    return value{v.value()};
  }
}

// Returns nullopt when the value holds a different alternative: a protocol
// write that carries the wrong type must be rejected, not reinterpreted.
template<some_parameter_type T>
constexpr auto from_value(value const& v) -> std::optional<T>
{
  if (auto const* p = std::get_if<scalar_t<T>>(&v)) {
    return T(*p);
  }
  return std::nullopt;
}

// -- Typed value <-> storage cell --

template<some_parameter_type T>
constexpr auto to_raw(T const& v) -> raw_value
{
  using scalar = scalar_t<T>;
  scalar const s = [&] {
    if constexpr (some_value<T>) {
      return v;
    }
    else {
      return v.value();
    }
  }();

  if constexpr (std::same_as<scalar, bool>) {
    return s ? raw_value{1} : raw_value{0};
  }
  else if constexpr (std::same_as<scalar, raw_value>) {
    return s;
  }
  else {
    return std::bit_cast<raw_value>(s);
  }
}

// Total, unlike from_value: a cell read back from storage may hold anything,
// and every bit pattern must yield a value rather than a trap. A bool is
// any-non-zero rather than a bit_cast, so a corrupted cell cannot produce a
// bool that is neither true nor false; a float may come back NaN, which the
// range check then rejects.
template<some_parameter_type T>
constexpr auto from_raw(raw_value r) -> T
{
  using scalar = scalar_t<T>;
  scalar const s = [r] {
    if constexpr (std::same_as<scalar, bool>) {
      return r != 0;
    }
    else if constexpr (std::same_as<scalar, raw_value>) {
      return r;
    }
    else {
      return std::bit_cast<scalar>(r);
    }
  }();
  return T(s);
}

// -- Type-erased value <-> storage cell --

constexpr auto to_raw(value const& v) -> raw_value
{
  return v.visit([](auto const& x) { return to_raw(x); });
}

constexpr auto to_value(value_type type, raw_value r) -> value
{
  switch (type) {
  case value_type::boolean: return from_raw<bool>(r);
  case value_type::int32: return from_raw<std::int32_t>(r);
  case value_type::uint32: return from_raw<std::uint32_t>(r);
  case value_type::float32: return from_raw<float>(r);
  }
  return from_raw<std::uint32_t>(r);
}

// Ordering of cells under their type tag: bit patterns are not ordered, the
// values they encode are — as int32 the cell 0xFFFFFFFB is -5 and compares
// below 10, while as a raw word it is above it.
//
// A NaN compares false against everything, so a float restored from a
// corrupted cell falls outside every range — which is what it should do. A
// bool cell holds 0 or 1; any other word is corruption and compares outside
// [false, true], even though from_raw would read it as true.
constexpr bool less_equal(value_type type, raw_value a, raw_value b)
{
  switch (type) {
  case value_type::boolean:
  case value_type::uint32: return a <= b;
  case value_type::int32: {
    using i32 = std::int32_t;
    return from_raw<i32>(a) <= from_raw<i32>(b);
  }
  case value_type::float32: return from_raw<float>(a) <= from_raw<float>(b);
  }
  return false;
}

constexpr bool
in_range(value_type type, raw_value v, raw_value lo, raw_value hi)
{
  return less_equal(type, lo, v) && less_equal(type, v, hi);
}

} // namespace settings
} // namespace emb
