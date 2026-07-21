#pragma once

#include <concepts>
#include <type_traits>
#include <variant>

namespace emb {

// T is one of Variant's std::variant alternatives.
template<typename T, typename Variant>
struct is_alternative_of : std::false_type {};

template<typename T, typename... Ts>
struct is_alternative_of<T, std::variant<Ts...>>
    : std::bool_constant<(... || std::same_as<T, Ts>)> {};

template<typename T, typename Variant>
inline constexpr bool is_alternative_of_v =
    is_alternative_of<T, Variant>::value;

template<typename T, typename Variant>
concept alternative_of = is_alternative_of_v<T, Variant>;

} // namespace emb
