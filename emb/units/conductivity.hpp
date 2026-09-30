#pragma once

#include <emb/units/named_unit.hpp>

#include <concepts>

namespace emb {
namespace units {

namespace tags {

// Unit tag of the microsiemens per centimeter.
struct microsiemens_per_cm {};

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
