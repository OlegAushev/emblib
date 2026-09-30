#pragma once

#include <emb/units/named_unit.hpp>

#include <concepts>

namespace emb {
namespace units {

namespace tags {

// Unit tag of the ampere.
struct amp {};

// Unit tag of the volt.
struct volt {};

// Unit tag of the ohm.
struct ohm {};

} // namespace tags

// `amp` is an alias template for a `named_unit` that holds an electric current
// in amperes.
template<std::floating_point T>
using amp = named_unit<T, tags::amp>;

// `volt` is an alias template for a `named_unit` that holds a voltage in volts.
template<std::floating_point T>
using volt = named_unit<T, tags::volt>;

// `ohm` is an alias template for a `named_unit` that holds a resistance in
// ohms.
template<std::floating_point T>
using ohm = named_unit<T, tags::ohm>;

// `amp_f32` is `amp` with a numerical value of type `float`.
using amp_f32 = amp<float>;

// `volt_f32` is `volt` with a numerical value of type `float`.
using volt_f32 = volt<float>;

// `ohm_f32` is `ohm` with a numerical value of type `float`.
using ohm_f32 = ohm<float>;

} // namespace units
} // namespace emb
