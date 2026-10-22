#pragma once

#include <emb/meta/same_as_any.hpp>

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace emb {

namespace detail {

// Whether a 32-bit scalar of the signedness of the integral type `T` holds
// every value of `T`, as `enum_carrier32_t` carries the values of an
// enumeration of `T`: `T` is an integer of at most 32 bits, and a number, not
// `bool` and not a character type. `signed char` and `unsigned char`, as
// `std::int8_t` and `std::uint8_t`, are numbers here.
template<typename T>
concept fits_carrier32 =
    std::integral<T>
    && (sizeof(T) <= 4)
    && !same_as_any<T, bool, char, wchar_t, char8_t, char16_t, char32_t>;

} // namespace detail

// The 32-bit scalar that carries a value of the enumeration `E` where values
// travel or are stored as 32-bit scalars, e.g. on a CANopen bus or in a cell
// of settings: `std::uint32_t` if the underlying type of `E` is unsigned,
// `std::int32_t` if it is signed, whatever its width, so that every value of
// `E` fits it. An enumeration of `int` is thus carried as `std::int32_t` also
// where the two differ, e.g. on Arm, where `std::int32_t` is `long`. It names
// a type only if the underlying type of `E` fits it
// (`detail::fits_carrier32`): an integer of at most 32 bits, not `bool` and
// not a character type; `requires { typename enum_carrier32_t<E>; }` tells
// whether `E` can be carried.
template<typename E>
  requires std::is_enum_v<E>
        && detail::fits_carrier32<std::underlying_type_t<E>>
using enum_carrier32_t =
    std::conditional_t<std::is_signed_v<std::underlying_type_t<E>>,
                       std::int32_t,
                       std::uint32_t>;

} // namespace emb
