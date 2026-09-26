#pragma once

#include <type_traits>

namespace emb {

// Provides the member constant `value` equal to the zero-based index of the
// first type in `Ts` that is the same type as `T`, taking into account
// cv-qualification and references. The program is ill-formed if there is no
// such type.
template<typename T, typename... Ts>
struct type_index;

template<typename T, typename... Ts>
struct type_index<T, T, Ts...> : std::integral_constant<std::size_t, 0> {};

template<typename T, typename U, typename... Ts>
struct type_index<T, U, Ts...>
    : std::integral_constant<std::size_t, 1 + type_index<T, Ts...>::value> {};

// Helper variable template for `type_index`.
template<typename T, typename... Ts>
inline constexpr std::size_t type_index_v = type_index<T, Ts...>::value;

} // namespace emb
