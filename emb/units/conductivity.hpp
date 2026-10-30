#pragma once

#include <emb/units/quantity.hpp>

#include <emb/i18n.hpp>

#include <concepts>

namespace emb {
namespace units {

namespace tags {

// Unit tag of the microsiemens per centimeter, whose symbol is "µS/cm",
// with U+00B5, the micro sign, in Russian "мкСм/см".
struct microsiemens_per_cm {
  static constexpr i18n::text symbol = {"µS/cm", "мкСм/см"};
};

} // namespace tags

// `microsiemens_per_cm` is an alias template for a `quantity` that holds an
// electrical conductivity in microsiemens per centimeter.
template<std::floating_point T>
using microsiemens_per_cm = quantity<T, tags::microsiemens_per_cm>;

// `microsiemens_per_cm_f32` is `microsiemens_per_cm` with a numerical value of
// type `float`.
using microsiemens_per_cm_f32 = microsiemens_per_cm<float>;

} // namespace units
} // namespace emb
