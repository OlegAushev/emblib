#pragma once

#include "od.hpp"

#include <emb/meta/wrapper.hpp>

#include <concepts>
#include <optional>
#include <variant>

namespace emb {
namespace can {
namespace canopen {

// The concept `wraps_od_scalar<T>` is satisfied if and only if `T` is a
// wrapper (`some_wrapper`) of a scalar that satisfies `od_scalar`, e.g.
// `emb::units::named_unit` or `emb::clamped` of a `float`. A new wrapper needs
// no change here.
template<typename T>
concept wraps_od_scalar = some_wrapper<T> && od_scalar<unwrapped_t<T>>;

// Converts `v` to an `od_value`. If `T` satisfies `od_scalar`, the result
// holds `v`; if `T` satisfies `wraps_od_scalar`, it holds the scalar that
// `value_of(v)` returns. The program is ill-formed if `T` satisfies neither
// concept or if `sizeof(T)` is greater than 4.
template<typename T>
constexpr od_value to_od_value(T const& v)
{
  static_assert(sizeof(T) <= 4, "od_value holds only types of sizeof <= 4");
  if constexpr (od_scalar<T>) {
    return od_value{v};
  }
  else if constexpr (wraps_od_scalar<T>) {
    return od_value{value_of(v)};
  }
  else {
    static_assert(false, "unsupported od_value type");
  }
}

// Extracts a value of type T from od_value; returns std::nullopt if the held
// alternative does not match. A wrapped type is built from the alternative
// its scalar occupies.
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
  else {
    static_assert(false, "unsupported od_value type");
  }
  return std::nullopt;
}

} // namespace canopen
} // namespace can
} // namespace emb
