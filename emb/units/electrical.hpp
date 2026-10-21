#pragma once

#include <emb/units/named_unit.hpp>

#include <concepts>
#include <string_view>

namespace emb {
namespace units {

namespace tags {

// Unit tag of the ampere, whose symbol is "A".
struct amp {
  static constexpr std::string_view symbol = "A";
};

// Unit tag of the volt, whose symbol is "V".
struct volt {
  static constexpr std::string_view symbol = "V";
};

// Unit tag of the ohm, whose symbol is "Ω": U+03A9, the Greek capital letter
// omega, into which NFC normalization turns U+2126, the ohm sign.
struct ohm {
  static constexpr std::string_view symbol = "Ω";
};

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
