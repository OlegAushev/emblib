#pragma once

#include <concepts>
#include <type_traits>
#include <variant>

namespace emb {

// Checks whether `Variant` is a specialization of `std::variant` and `T` is the
// same type as one of its alternatives, taking into account cv-qualification
// and references. For any other `Variant`, e.g. `std::variant<T> const&`,
// `value` is `false`.
template<typename T, typename Variant>
struct is_alternative_of : std::false_type {};

template<typename T, typename... Ts>
struct is_alternative_of<T, std::variant<Ts...>>
    : std::bool_constant<(... || std::same_as<T, Ts>)> {};

// Helper variable template for `is_alternative_of`.
template<typename T, typename Variant>
inline constexpr bool is_alternative_of_v =
    is_alternative_of<T, Variant>::value;

// The concept `alternative_of<T, Variant>` is satisfied if and only if
// `Variant` is a specialization of `std::variant` and `T` is the same type as
// one of its alternatives, taking into account cv-qualification and references.
// It is not satisfied for any other `Variant`, e.g. `std::variant<T> const&`.
template<typename T, typename Variant>
concept alternative_of = is_alternative_of_v<T, Variant>;

} // namespace emb
