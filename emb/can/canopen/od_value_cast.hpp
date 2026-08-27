#pragma once

#include "od.hpp"

#include <emb/units/named_unit.hpp>

#include <optional>
#include <variant>

namespace emb {
namespace can {
namespace canopen {

// Wraps a value of arbitrary type into od_value. Handles od_value scalars
// as-is and named_unit types via `.value()`.
template<typename T>
constexpr auto to_od_value(T const& v) -> od_value {
  static_assert(sizeof(T) <= 4, "od_value holds only types of sizeof <= 4");
  if constexpr (od_scalar<T>) {
    return od_value{v};
  } else if constexpr (units::unit<T>) {
    return od_value{v.value()};
  } else {
    static_assert(false, "unsupported od_value type");
  }
}

// Extracts a value of type T from od_value; returns std::nullopt if the held
// alternative does not match. Named_unit types are constructed from the float
// alternative.
template<typename T>
constexpr auto from_od_value(od_value const& val) -> std::optional<T> {
  static_assert(sizeof(T) <= 4, "od_value holds only types of sizeof <= 4");
  if constexpr (od_scalar<T>) {
    if (auto* p = std::get_if<T>(&val)) {
      return *p;
    }
  } else if constexpr (units::unit<T>) {
    if (auto* p = std::get_if<float>(&val)) {
      return T{*p};
    }
  } else {
    static_assert(false, "unsupported od_value type");
  }
  return std::nullopt;
}

} // namespace canopen
} // namespace can
} // namespace emb
