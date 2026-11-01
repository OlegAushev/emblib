#pragma once

#include <emb/meta/nth_type.hpp>

#include <concepts>
#include <type_traits>

#include <cstddef>

namespace emb {

// The class template `typelist` represents a compile-time sequence of types.
// When used as an argument to a function template, the pack `Ts` can be
// deduced and used in pack expansion. The static member `size` is the number
// of elements, i.e. `sizeof...(Ts)`.
template<typename... Ts>
struct typelist {
  static constexpr std::size_t size = sizeof...(Ts);
};

namespace detail {

template<typename T>
inline constexpr bool is_typelist = false;

template<typename... Ts>
inline constexpr bool is_typelist<typelist<Ts...>> = true;

} // namespace detail

// The concept `typelist_specialization<T>` is satisfied if and only if `T` is
// a specialization of `typelist`. A cv-qualified specialization, e.g.
// `typelist<> const`, does not satisfy it.
template<typename T>
concept typelist_specialization = detail::is_typelist<T>;

// Provides the member constant `value` equal to the number of elements of the
// typelist `T`. The program is ill-formed if `T` is not a typelist.
template<typename T>
struct typelist_size;

template<typename... Ts>
struct typelist_size<typelist<Ts...>> {
  static constexpr std::size_t value = sizeof...(Ts);
};

// Helper variable template for `typelist_size`.
template<typename List>
inline constexpr std::size_t typelist_size_v = typelist_size<List>::value;

namespace detail {

template<typename T, typename List>
inline constexpr bool is_listed_in = false;

template<typename T, typename... Ts>
inline constexpr bool is_listed_in<T, typelist<Ts...>> =
    (... || std::same_as<T, Ts>);

} // namespace detail

// The concept `listed_in<T, List>` is satisfied if and only if `List`
// satisfies `typelist_specialization` and `T` is an element of `List`.
template<typename T, typename List>
concept listed_in =
    typelist_specialization<List> && detail::is_listed_in<T, List>;

// Provides the member typedef `type`, which is the element at zero-based index
// `I` of the typelist `List`. The program is ill-formed if `List` is not a
// typelist or `I >= List::size`.
template<typename List, std::size_t I>
struct typelist_at;

template<typename... Ts, std::size_t I>
struct typelist_at<typelist<Ts...>, I> {
  using type = nth_type_t<I, Ts...>;
};

// Helper alias template for `typelist_at`.
template<typename List, std::size_t I>
using typelist_at_t = typename typelist_at<List, I>::type;

// Provides the member constant `value` equal to the number of elements of the
// typelist `List` that are the same type as `T`. The program is ill-formed if
// `List` is not a typelist.
template<typename List, typename T>
struct typelist_count_t;

template<typename... Ts, typename T>
struct typelist_count_t<typelist<Ts...>, T>
    : std::integral_constant<std::size_t,
                             (0 + ... + (std::same_as<T, Ts> ? 1 : 0))> {};

// Helper variable template for `typelist_count_t`.
template<typename List, typename T>
inline constexpr std::size_t typelist_count_v =
    typelist_count_t<List, T>::value;

namespace detail {

template<typename List>
inline constexpr bool is_duplicate_free = false;

template<typename... Ts>
inline constexpr bool is_duplicate_free<typelist<Ts...>> =
    (... && (typelist_count_v<typelist<Ts...>, Ts> == 1));

} // namespace detail

// The concept `duplicate_free_typelist<List>` is satisfied if and only if
// `List` satisfies `typelist_specialization` and no two elements of `List` are
// the same type, which holds for an empty typelist.
template<typename List>
concept duplicate_free_typelist =
    typelist_specialization<List> && detail::is_duplicate_free<List>;

// Provides the member typedef `type`, which is the typelist `List` with `T`
// added as its last element, even if `T` is itself a typelist. The program is
// ill-formed if `List` is not a typelist.
template<typename List, typename T>
struct typelist_append;

template<typename... Types, typename T>
struct typelist_append<typelist<Types...>, T> {
  using type = typelist<Types..., T>;
};

// Helper alias template for `typelist_append`.
template<typename List, typename T>
using typelist_append_t = typename typelist_append<List, T>::type;

// Provides the member typedef `type`, which is a typelist of the elements of
// the typelist `A` followed by the elements of the typelist `B`. The program is
// ill-formed if `A` or `B` is not a typelist.
template<typename A, typename B>
struct typelist_concat;

template<typename... As, typename... Bs>
struct typelist_concat<typelist<As...>, typelist<Bs...>> {
  using type = typelist<As..., Bs...>;
};

// Helper alias template for `typelist_concat`.
template<typename A, typename B>
using typelist_concat_t = typename typelist_concat<A, B>::type;

} // namespace emb
