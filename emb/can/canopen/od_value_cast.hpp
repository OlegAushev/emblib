#pragma once

#include "od.hpp"

#include <emb/meta/enum_carrier32.hpp>
#include <emb/meta/scalar_wrapper.hpp>

#include <concepts>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace emb {
namespace can {
namespace canopen {

// The concept `wraps_od_scalar<T>` is satisfied if and only if `T` is a
// scalar wrapper (`scalar_wrapper`) of a type that satisfies `od_scalar`, e.g.
// `emb::units::quantity` or `emb::clamped` of a `float`. A new wrapper needs
// no change here.
template<typename T>
concept wraps_od_scalar = scalar_wrapper<T> && od_scalar<unwrapped_t<T>>;

// The concept `od_enumeration<T>` is satisfied if and only if `T` is an
// enumeration whose values travel as their carrier, `emb::enum_carrier32_t`:
// one whose underlying type is an integer of at most 32 bits, not `bool` and
// not a character type. Whether it labels its values, as a value on the bus
// must, is for the builders of bindings to check.
template<typename T>
concept od_enumeration =
    std::is_enum_v<T> && requires { typename enum_carrier32_t<T>; };

namespace detail {

template<typename T>
struct od_carrier {
  using type = unwrapped_t<T>;
};

template<od_enumeration T>
struct od_carrier<T> {
  using type = enum_carrier32_t<T>;
};

} // namespace detail

// The scalar that a value of type `T` travels as: `T` itself if `T` satisfies
// `od_scalar`, the scalar it wraps if it satisfies `wraps_od_scalar`, and its
// carrier, `std::uint32_t` or `std::int32_t`, if it satisfies
// `od_enumeration`. For any other `T`, `T` itself, which no object takes.
template<typename T>
using od_carrier_t = typename detail::od_carrier<T>::type;

// Converts `v` to an `od_value`. If `T` satisfies `od_scalar`, the result
// holds `v`; if `T` satisfies `wraps_od_scalar`, it holds the scalar that
// `scalar_of(v)` returns; if `T` satisfies `od_enumeration`, it holds the
// underlying value of `v` in its carrier. The program is ill-formed if `T`
// satisfies none of these concepts or if `sizeof(T)` is greater than 4.
template<typename T>
constexpr od_value to_od_value(T const& v)
{
  static_assert(sizeof(T) <= 4, "od_value holds only types of sizeof <= 4");
  if constexpr (od_scalar<T>) {
    return od_value{v};
  }
  else if constexpr (wraps_od_scalar<T>) {
    return od_value{scalar_of(v)};
  }
  else if constexpr (od_enumeration<T>) {
    od_carrier_t<T> const carried = std::to_underlying(v);
    return od_value{carried};
  }
  else {
    static_assert(false, "unsupported od_value type");
  }
}

// Extracts a value of type T from od_value; returns std::nullopt if the held
// alternative does not match. A wrapped type is built from the alternative
// its scalar occupies, and an enumeration from the alternative of its
// carrier, if the value there is one of its underlying type: the value is
// checked before it is converted, so that 260 is not taken for 4 by an
// enumeration of `std::uint8_t`.
template<typename T>
constexpr std::optional<T> from_od_value(od_value const& val)
{
  static_assert(sizeof(T) <= 4, "od_value holds only types of sizeof <= 4");
  if constexpr (od_scalar<T>) {
    if (auto* p = std::get_if<T>(&val)) {
      return *p;
    }
  }
  else if constexpr (wraps_od_scalar<T>) {
    if (auto* p = std::get_if<typename T::value_type>(&val)) {
      return T{*p};
    }
  }
  else if constexpr (od_enumeration<T>) {
    if (auto* p = std::get_if<od_carrier_t<T>>(&val)) {
      if (std::in_range<std::underlying_type_t<T>>(*p)) {
        return static_cast<T>(*p);
      }
    }
  }
  else {
    static_assert(false, "unsupported od_value type");
  }
  return std::nullopt;
}

} // namespace canopen
} // namespace can
} // namespace emb
