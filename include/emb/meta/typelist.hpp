#pragma once

#include <concepts>
#include <type_traits>

#include <cstddef>

namespace emb {

template<typename... Ts>
struct typelist {};

template<typename T>
struct typelist_size;

template<typename... Ts>
struct typelist_size<typelist<Ts...>> {
  static constexpr size_t value = sizeof...(Ts);
};

template<typename List, typename T>
struct typelist_contains_type : std::false_type {};

template<typename... Ts, typename T>
struct typelist_contains_type<typelist<Ts...>, T>
    : std::bool_constant<(... || std::same_as<T, Ts>)> {};

template<typename List, typename T>
inline constexpr bool typelist_contains_v =
    typelist_contains_type<List, T>::value;

template<typename List, typename T>
concept typelist_contains = typelist_contains_v<List, T>;

template<typename List, typename T>
struct typelist_append;

template<typename... Types, typename T>
struct typelist_append<typelist<Types...>, T> {
  using type = typelist<Types..., T>;
};

} // namespace emb
