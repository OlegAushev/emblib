#pragma once

#include <emb/units/quantity.hpp>

#include <emb/i18n.hpp>

#include <concepts>

namespace emb {
namespace units {

namespace tags {

// Unit tag of the ampere, whose symbol is "A", in Russian "А": U+0410, the
// Cyrillic capital letter A.
struct amp {
  static constexpr i18n::text symbol = {"A", "А"};
};

// Unit tag of the volt, whose symbol is "V", in Russian "В".
struct volt {
  static constexpr i18n::text symbol = {"V", "В"};
};

// Unit tag of the ohm, whose symbol is "Ω": U+03A9, the Greek capital letter
// omega, into which NFC normalization turns U+2126, the ohm sign; in Russian
// "Ом".
struct ohm {
  static constexpr i18n::text symbol = {"Ω", "Ом"};
};

} // namespace tags

// `amp` is an alias template for a `quantity` that holds an electric current
// in amperes.
template<std::floating_point T>
using amp = quantity<T, tags::amp>;

// `volt` is an alias template for a `quantity` that holds a voltage in volts.
template<std::floating_point T>
using volt = quantity<T, tags::volt>;

// `ohm` is an alias template for a `quantity` that holds a resistance in ohms.
template<std::floating_point T>
using ohm = quantity<T, tags::ohm>;

// `amp_f32` is `amp` with a numerical value of type `float`.
using amp_f32 = amp<float>;

// `volt_f32` is `volt` with a numerical value of type `float`.
using volt_f32 = volt<float>;

// `ohm_f32` is `ohm` with a numerical value of type `float`.
using ohm_f32 = ohm<float>;

} // namespace units
} // namespace emb
