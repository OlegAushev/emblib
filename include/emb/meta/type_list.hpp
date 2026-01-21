#pragma once

#include <concepts>
#include <type_traits>

#include <cstddef>

namespace emb {

template<typename... Ts>
struct type_list {};

template<typename T, typename List>
struct is_in_type_list : std::false_type {};

template<typename T, typename... Ts>
struct is_in_type_list<T, type_list<Ts...>>
    : std::bool_constant<(... || std::is_same_v<T, Ts>)> {};

template<typename T, typename List>
inline constexpr bool is_in_type_list_v = is_in_type_list<T, List>::value;

template<typename T, typename List>
concept in_type_list = is_in_type_list_v<T, List>;

template<typename T>
struct type_list_size;

template<typename... Ts>
struct type_list_size<type_list<Ts...>> {
    static constexpr size_t value = sizeof...(Ts);
};

template <typename List, typename T>
struct type_list_append;

template <typename T, typename ...Types>
struct type_list_append<type_list<Types...>, T> {
    using type = type_list<Types..., T>;
};

} // namespace emb
