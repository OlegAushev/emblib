#pragma once

#include <type_traits>

namespace emb {

template<typename T, typename... Ts>
struct type_index;

template<typename T, typename... Ts>
struct type_index<T, T, Ts...> : std::integral_constant<std::size_t, 0> {};

template<typename T, typename U, typename... Ts>
struct type_index<T, U, Ts...>
    : std::integral_constant<std::size_t, 1 + type_index<T, Ts...>::value> {};

template<typename T, typename... Ts>
inline constexpr std::size_t type_index_v = type_index<T, Ts...>::value;

} // namespace emb
