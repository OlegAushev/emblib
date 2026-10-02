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

// The scoped enumeration `value_type` names the scalar types a parameter may
// have: `bool`, `std::int32_t`, `std::uint32_t` and `float`. The value of
// each enumerator is the index of its type among the alternatives of `value`.
// Changing the value of an enumerator changes the identifier of every
// parameter of its type, so the cells stored before no longer match and those
// parameters load their defaults.
enum class value_type : std::uint8_t {
  boolean,
  int32,
  uint32,
  float32,
};

// Type-erased value of a parameter: the form in which code that does not
// know the type of the parameter, e.g. a transport, reads and writes it.
using value = std::variant<bool, std::int32_t, std::uint32_t, float>;

// Type of a cell, i.e. the four-byte encoding of the value of one parameter,
// whatever the type of the parameter. `to_raw` defines the encoding.
using raw_value = std::uint32_t;

// The concept `some_value<T>` is satisfied if and only if `T` is exactly one
// of the alternatives of `value`: `bool`, `std::int32_t`, `std::uint32_t` or
// `float`, not cv-qualified and not a reference.
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

// The concept `some_wrapped_value<T>` is satisfied if and only if `T` wraps
// one scalar that satisfies `some_value` and can be rebuilt from it:
// `T::value_type` names the scalar type, `T` is constructible from
// `T::value_type`, and `value_of` called with a `T const` lvalue returns
// `T::value_type` by value.
//
// A wrapper, e.g. `emb::units::named_unit` or `emb::clamped`, opts in by
// declaring a `value_of` overload that argument-dependent lookup finds: in
// its own namespace or as a friend defined in the class. Only the scalar of a
// wrapper is stored: conversions in this header go through `value_of` and
// the constructor, never through the bytes of an object, so the layout of a
// wrapper does not matter.
template<typename T>
concept some_wrapped_value = requires { typename T::value_type; }
                          && some_value<typename T::value_type>
                          && std::constructible_from<T, typename T::value_type>
                          && requires(T const& v) {
                               {
                                 value_of(v)
                               } -> std::same_as<typename T::value_type>;
                             };

// The concept `some_parameter_type<T>` is satisfied if and only if `T`
// satisfies `some_value` or `some_wrapped_value`, i.e. `T` is a type that a
// parameter may have.
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

// Scalar type in which a parameter of type `T` is stored and transported:
// `T` itself if `T` satisfies `some_value`, otherwise `T::value_type`.
template<some_parameter_type T>
using scalar_t = typename detail::scalar_of<T>::type;

// The `value_type` of a parameter of type `T`, i.e. the enumerator that
// names `scalar_t<T>`.
template<some_parameter_type T>
inline constexpr value_type type_of = detail::tag_of<scalar_t<T>>();

// Returns the `value_type` of the alternative that `v` holds.
constexpr value_type held_type(value const& v)
{
  return static_cast<value_type>(v.index());
}

// -- Typed value <-> type-erased value --

// Returns `v` in type-erased form: a `value` that holds `v` itself if `T`
// satisfies `some_value`, otherwise `value_of(v)`.
template<some_parameter_type T>
constexpr value to_value(T const& v)
{
  if constexpr (some_value<T>) {
    return value{v};
  }
  else {
    return value{value_of(v)};
  }
}

// Converts `v` back to the parameter type `T`. If `v` holds a value of type
// `scalar_t<T>`, returns `T` constructed from it; otherwise returns
// `std::nullopt`: a value of another scalar type is neither converted nor
// reinterpreted.
template<some_parameter_type T>
constexpr std::optional<T> from_value(value const& v)
{
  if (auto const* p = std::get_if<scalar_t<T>>(&v)) {
    return T(*p);
  }
  return std::nullopt;
}

// -- Typed value <-> storage cell --

// Returns the cell that encodes `v`: `false` and `true` as 0 and 1, any other
// scalar as its bit pattern, as if by `std::bit_cast<raw_value>`. A wrapper
// is encoded as its scalar, `value_of(v)`.
template<some_parameter_type T>
constexpr raw_value to_raw(T const& v)
{
  using scalar = scalar_t<T>;
  scalar const s = [&] {
    if constexpr (some_value<T>) {
      return v;
    }
    else {
      return value_of(v);
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

// Decodes the cell `r` as a value of the parameter type `T`, i.e. the inverse
// of `to_raw`. Every bit pattern decodes to a scalar: for `bool`, any non-zero
// `r` decodes as `true`; for any other scalar, `r` is its bit pattern, so a
// `float` may decode as NaN. If `T` is a wrapper, returns `T` constructed
// from that scalar.
template<some_parameter_type T>
constexpr T from_raw(raw_value r)
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

// Returns the cell that encodes `v`, i.e. `to_raw` applied to the scalar that
// `v` holds.
constexpr raw_value to_raw(value const& v)
{
  return v.visit([](auto const& x) { return to_raw(x); });
}

// Decodes the cell `r` as a value of the scalar type that `type` names, as
// `from_raw` does, and returns it in type-erased form.
constexpr value to_value(value_type type, raw_value r)
{
  switch (type) {
  case value_type::boolean: return from_raw<bool>(r);
  case value_type::int32: return from_raw<std::int32_t>(r);
  case value_type::uint32: return from_raw<std::uint32_t>(r);
  case value_type::float32: return from_raw<float>(r);
  }
  return from_raw<std::uint32_t>(r);
}

// Checks whether the cell `a` is less than or equal to the cell `b`, both
// holding values of the scalar type that `type` names. Cells of
// `value_type::int32` and `value_type::float32` are compared as the numbers
// they encode, e.g. as `value_type::int32` the cell `0xFFFFFFFB` encodes -5
// and compares below the cell of 10; cells of `value_type::boolean` and
// `value_type::uint32` are compared as unsigned words.
//
// Returns `false` if `type` is `value_type::float32` and either cell holds a
// NaN. A `value_type::boolean` cell other than 0 and 1 compares above the
// cell of `true`, although `from_raw` decodes it as `true`.
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

// Checks whether the cell `v` lies in [`lo`, `hi`], with the cells compared
// as `less_equal` compares them under `type`. Hence a `value_type::float32`
// cell that holds a NaN is in no range, and a `value_type::boolean` cell
// other than 0 and 1 is in no range whose bounds encode `bool` values.
constexpr bool
in_range(value_type type, raw_value v, raw_value lo, raw_value hi)
{
  return less_equal(type, lo, v) && less_equal(type, v, hi);
}

} // namespace settings
} // namespace emb
