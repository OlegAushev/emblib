#pragma once

#include <concepts>
#include <type_traits>

#include <cstddef>

namespace emb {

template<typename... Ts>
struct typelist {};

// -- size --

template<typename T>
struct typelist_size;

template<typename... Ts>
struct typelist_size<typelist<Ts...>> {
  static constexpr size_t value = sizeof...(Ts);
};

template<typename List>
inline constexpr size_t typelist_size_v = typelist_size<List>::value;

// -- contains --

template<typename List, typename T>
struct typelist_contains_t : std::false_type {};

template<typename... Ts, typename T>
struct typelist_contains_t<typelist<Ts...>, T>
    : std::bool_constant<(... || std::same_as<T, Ts>)> {};

template<typename List, typename T>
inline constexpr bool typelist_contains_v = typelist_contains_t<List, T>::value;

template<typename List, typename T>
concept typelist_contains = typelist_contains_v<List, T>;

// -- at --

template<typename List, size_t I>
struct typelist_at;

template<typename... Ts, size_t I>
struct typelist_at<typelist<Ts...>, I> {
  using type = Ts...[I];
};

template<typename List, size_t I>
using typelist_at_t = typename typelist_at<List, I>::type;

// -- count --

template<typename List, typename T>
struct typelist_count_t;

template<typename... Ts, typename T>
struct typelist_count_t<typelist<Ts...>, T>
    : std::integral_constant<
          size_t,
          (0 + ... + (std::same_as<T, Ts> ? 1 : 0))> {};

template<typename List, typename T>
inline constexpr size_t typelist_count_v = typelist_count_t<List, T>::value;

// -- unique --

template<typename List>
struct typelist_unique_t;

template<typename... Ts>
struct typelist_unique_t<typelist<Ts...>>
    : std::bool_constant<
          (... && (typelist_count_v<typelist<Ts...>, Ts> == 1))> {};

template<typename List>
inline constexpr bool typelist_unique_v = typelist_unique_t<List>::value;

template<typename List>
concept typelist_unique = typelist_unique_v<List>;

// -- append --

template<typename List, typename T>
struct typelist_append;

template<typename... Types, typename T>
struct typelist_append<typelist<Types...>, T> {
  using type = typelist<Types..., T>;
};

template<typename List, typename T>
using typelist_append_t = typename typelist_append<List, T>::type;

} // namespace emb
