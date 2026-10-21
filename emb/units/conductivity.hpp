#pragma once

#include <emb/units/named_unit.hpp>

#include <concepts>
#include <string_view>

namespace emb {
namespace units {

namespace tags {

// Unit tag of the microsiemens per centimeter, whose symbol is "µS/cm",
// with U+00B5, the micro sign.
struct microsiemens_per_cm {
  static constexpr std::string_view symbol = "µS/cm";
};

} // namespace tags

// `microsiemens_per_cm` is an alias template for a `named_unit` that holds an
// electrical conductivity in microsiemens per centimeter.
template<std::floating_point T>
using microsiemens_per_cm = named_unit<T, tags::microsiemens_per_cm>;

// `microsiemens_per_cm_f32` is `microsiemens_per_cm` with a numerical value of
// type `float`.
using microsiemens_per_cm_f32 = microsiemens_per_cm<float>;

} // namespace units
} // namespace emb
