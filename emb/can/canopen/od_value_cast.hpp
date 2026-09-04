#pragma once

#include "od.hpp"

#include <concepts>
#include <optional>
#include <variant>

namespace emb {
namespace can {
namespace canopen {

// A type that wraps one scalar and can be rebuilt from it: units::named_unit,
// emb::clamped, and anything else shaped like them. Recognised structurally
// rather than by name, so a new wrapper needs no change here — the same
// shape emb::settings::some_wrapped_value describes.
template<typename T>
concept wraps_od_scalar = requires { typename T::value_type; }
                       && od_scalar<typename T::value_type>
                       && std::constructible_from<T, typename T::value_type>
                       && requires(T const& v) {
                            {
                              v.value()
                            } -> std::same_as<typename T::value_type>;
                          };

// Wraps a value of arbitrary type into od_value. Handles od_value scalars
// as-is and wrapped ones via `.value()`.
template<typename T>
constexpr auto to_od_value(T const& v) -> od_value
{
  static_assert(sizeof(T) <= 4, "od_value holds only types of sizeof <= 4");
  if constexpr (od_scalar<T>) {
    return od_value{v};
  }
  else if constexpr (wraps_od_scalar<T>) {
    return od_value{v.value()};
  }
  else {
    static_assert(false, "unsupported od_value type");
  }
}

// Extracts a value of type T from od_value; returns std::nullopt if the held
// alternative does not match. A wrapped type is built from the alternative
// its scalar occupies.
template<typename T>
constexpr auto from_od_value(od_value const& val) -> std::optional<T>
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
