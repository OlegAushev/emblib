#pragma once

#include <type_traits>

namespace emb {

// Primary template - type not found case
template<typename T, typename... Ts>
struct type_index;

// Base case: type found at position 0
template<typename T, typename... Ts>
struct type_index<T, T, Ts...> : std::integral_constant<std::size_t, 0> {};

// Recursive case: increment index and continue searching
template<typename T, typename U, typename... Ts>
struct type_index<T, U, Ts...>
    : std::integral_constant<std::size_t, 1 + type_index<T, Ts...>::value> {};

// Helper template
template<typename T, typename... Ts>
inline constexpr std::size_t type_index_v = type_index<T, Ts...>::value;

} // namespace emb
