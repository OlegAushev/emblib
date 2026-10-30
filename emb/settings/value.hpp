#pragma once

#include <emb/meta/alternative_of.hpp>
#include <emb/meta/enum_carrier32.hpp>
#include <emb/meta/enum_info.hpp>
#include <emb/meta/scalar_wrapper.hpp>

#include <bit>
#include <concepts>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

#include <cstdint>

namespace emb {
namespace settings {

// The scoped enumeration `scalar_kind` names the scalar types a parameter may
// have: `bool`, `std::int32_t`, `std::uint32_t` and `float`. The value of
// each enumerator is the index of its type among the alternatives of `value`.
// Changing the value of an enumerator changes the identifier of every
// parameter of its kind, so the cells stored before no longer match and those
// parameters load their defaults.
enum class scalar_kind : std::uint8_t {
  boolean,
  int32,
  uint32,
  float32,
};

// Type-erased value of a parameter: the form in which code that does not
// know the type of the parameter, e.g. a transport, reads and writes it.
using value = std::variant<bool, std::int32_t, std::uint32_t, float>;

// Type of a cell, i.e. the four-byte encoding of the value of one parameter,
// whatever the type of the parameter. `to_cell` defines the encoding.
using cell = std::uint32_t;

// The concept `some_value<T>` is satisfied if and only if `T` is exactly one
// of the alternatives of `value`: `bool`, `std::int32_t`, `std::uint32_t` or
// `float`, not cv-qualified and not a reference.
template<typename T>
concept some_value = alternative_of<T, value>;

// The enumerators of `scalar_kind` are the alternative indices of `value`, so
// `kind_of` and `held_kind` below are casts of an index and the two cannot
// silently drift apart.
static_assert(std::variant_size_v<value> == 4);
static_assert(std::same_as<std::variant_alternative_t<0, value>, bool>);
static_assert(std::same_as<std::variant_alternative_t<1, value>, std::int32_t>);
static_assert(
    std::same_as<std::variant_alternative_t<2, value>, std::uint32_t>);
static_assert(std::same_as<std::variant_alternative_t<3, value>, float>);
static_assert(std::to_underlying(scalar_kind::float32) == 3);

// The concept `some_wrapped_value<T>` is satisfied if and only if `T` is a
// scalar wrapper (`scalar_wrapper`) of a type that satisfies `some_value`,
// e.g. `emb::units::quantity` or `emb::clamped` of a `float`. Only the scalar
// of a wrapper is stored: conversions in this header go through `value_of` and
// the constructor, never through the bytes of an object, so the layout of a
// wrapper does not matter.
template<typename T>
concept some_wrapped_value = scalar_wrapper<T> && some_value<unwrapped_t<T>>;

// The concept `some_enum_value<T>` is satisfied if and only if `T` is a
// scoped enumeration that labels its values (`emb::enum_with_info`) and
// whose values a 32-bit carrier holds (`emb::enum_carrier32_t`). A parameter
// of such a `T` is stored and transported as its carrier, a `std::uint32_t`
// or a `std::int32_t` as the underlying type of `T` is unsigned or signed,
// and takes only the values that `T` labels (`param`).
template<typename T>
concept some_enum_value =
    enum_with_info<T> && requires { typename enum_carrier32_t<T>; };

// The concept `some_parameter_type<T>` is satisfied if and only if `T`
// satisfies `some_value`, `some_wrapped_value` or `some_enum_value`, i.e. `T`
// is a type that a parameter may have.
template<typename T>
concept some_parameter_type =
    some_value<T> || some_wrapped_value<T> || some_enum_value<T>;

namespace detail {

template<typename T>
struct scalar {
  using type = unwrapped_t<T>;
};

template<some_enum_value T>
struct scalar<T> {
  using type = enum_carrier32_t<T>;
};

} // namespace detail

// Scalar type in which a parameter of type `T` is stored and transported:
// `T` itself if `T` satisfies `some_value`, its carrier
// (`emb::enum_carrier32_t`) if `T` satisfies `some_enum_value`, otherwise
// `T::value_type`.
template<some_parameter_type T>
using scalar_t = typename detail::scalar<T>::type;

// The `scalar_kind` of a parameter of type `T`, i.e. the enumerator that
// names `scalar_t<T>`.
template<some_parameter_type T>
inline constexpr scalar_kind kind_of =
    static_cast<scalar_kind>(value(std::in_place_type<scalar_t<T>>).index());

// Returns the `scalar_kind` of the alternative that `v` holds.
constexpr scalar_kind held_kind(value const& v)
{
  return static_cast<scalar_kind>(v.index());
}

// -- Typed value <-> type-erased value --

// Returns `v` in type-erased form: a `value` that holds `v` itself if `T`
// satisfies `some_value`, the underlying value of `v` in its carrier if `T`
// satisfies `some_enum_value`, otherwise `value_of(v)`.
template<some_parameter_type T>
constexpr value to_value(T const& v)
{
  if constexpr (some_value<T>) {
    return value{v};
  }
  else if constexpr (some_enum_value<T>) {
    scalar_t<T> const carried = std::to_underlying(v);
    return value{carried};
  }
  else {
    return value{value_of(v)};
  }
}

// Converts `v` back to the parameter type `T`. If `v` holds a value of type
// `scalar_t<T>`, returns `T` constructed from it; otherwise returns
// `std::nullopt`: a value of another scalar type is neither converted nor
// reinterpreted. An enumeration is returned only if the value is one of its
// underlying type, and `std::nullopt` otherwise: 260 does not become 4 for an
// enumeration of `std::uint8_t`.
template<some_parameter_type T>
constexpr std::optional<T> from_value(value const& v)
{
  if (auto const* p = std::get_if<scalar_t<T>>(&v)) {
    if constexpr (some_enum_value<T>) {
      if (!std::in_range<std::underlying_type_t<T>>(*p)) return std::nullopt;
    }
    return T(*p);
  }
  return std::nullopt;
}

// -- Typed value <-> storage cell --

// Returns the cell that encodes `v`: `false` and `true` as 0 and 1, any other
// scalar as its bit pattern, as if by `std::bit_cast<cell>`. A wrapper is
// encoded as its scalar, `value_of(v)`, and an enumeration as its underlying
// value in its carrier.
template<some_parameter_type T>
constexpr cell to_cell(T const& v)
{
  using scalar = scalar_t<T>;
  scalar const s = [&] {
    if constexpr (some_value<T>) {
      return v;
    }
    else if constexpr (some_enum_value<T>) {
      return scalar{std::to_underlying(v)};
    }
    else {
      return value_of(v);
    }
  }();

  if constexpr (std::same_as<scalar, bool>) {
    return s ? cell{1} : cell{0};
  }
  else {
    return std::bit_cast<cell>(s);
  }
}

// Decodes the cell `c` as a value of the parameter type `T`, i.e. the inverse
// of `to_cell`. Every bit pattern decodes to a scalar: for `bool`, any
// non-zero `c` decodes as `true`; for any other scalar, `c` is its bit
// pattern, so a `float` may decode as NaN. If `T` is a wrapper, returns `T`
// constructed from that scalar, and if `T` is an enumeration, the value of `T`
// that the scalar converts to: a value that `T` labels if the scalar lies in
// the bounds of the parameter, as an image keeps every cell.
template<some_parameter_type T>
constexpr T from_cell(cell c)
{
  using scalar = scalar_t<T>;
  scalar const s = [c] {
    if constexpr (std::same_as<scalar, bool>) {
      return c != 0;
    }
    else {
      return std::bit_cast<scalar>(c);
    }
  }();
  return T(s);
}

// -- Type-erased value <-> storage cell --

// Returns the cell that encodes `v`, i.e. `to_cell` applied to the scalar
// that `v` holds.
constexpr cell to_cell(value const& v)
{
  return v.visit([](auto const& x) { return to_cell(x); });
}

// Decodes the cell `c` as a value of the scalar type that `kind` names, as
// `from_cell` does, and returns it in type-erased form.
constexpr value to_value(scalar_kind kind, cell c)
{
  switch (kind) {
  case scalar_kind::boolean: return from_cell<bool>(c);
  case scalar_kind::int32: return from_cell<std::int32_t>(c);
  case scalar_kind::uint32: return from_cell<std::uint32_t>(c);
  case scalar_kind::float32: return from_cell<float>(c);
  }
  return from_cell<std::uint32_t>(c);
}

// Checks whether the cell `a` is less than or equal to the cell `b`, both
// holding values of the scalar type that `kind` names. Cells of
// `scalar_kind::int32` and `scalar_kind::float32` are compared as the numbers
// they encode, e.g. as `scalar_kind::int32` the cell `0xFFFFFFFB` encodes -5
// and compares below the cell of 10; cells of `scalar_kind::boolean` and
// `scalar_kind::uint32` are compared as unsigned words.
//
// Returns `false` if `kind` is `scalar_kind::float32` and either cell holds a
// NaN. A `scalar_kind::boolean` cell other than 0 and 1 compares above the
// cell of `true`, although `from_cell` decodes it as `true`.
constexpr bool less_equal(scalar_kind kind, cell a, cell b)
{
  switch (kind) {
  case scalar_kind::boolean:
  case scalar_kind::uint32: return a <= b;
  case scalar_kind::int32: {
    using i32 = std::int32_t;
    return from_cell<i32>(a) <= from_cell<i32>(b);
  }
  case scalar_kind::float32: return from_cell<float>(a) <= from_cell<float>(b);
  }
  return false;
}

// Checks whether the cell `v` lies in [`lo`, `hi`], with the cells compared
// as `less_equal` compares them under `kind`. Hence a `scalar_kind::float32`
// cell that holds a NaN is in no range, and a `scalar_kind::boolean` cell
// other than 0 and 1 is in no range whose bounds encode `bool` values.
constexpr bool in_range(scalar_kind kind, cell v, cell lo, cell hi)
{
  return less_equal(kind, lo, v) && less_equal(kind, v, hi);
}

} // namespace settings
} // namespace emb
