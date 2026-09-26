#pragma once

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

// Checks whether `T` is a specialization of `typelist`. For a cv-qualified
// specialization, e.g. `typelist<> const`, `value` is `false`.
template<typename T>
struct is_typelist : std::false_type {};

template<typename... Ts>
struct is_typelist<typelist<Ts...>> : std::true_type {};

// Helper variable template for `is_typelist`.
template<typename T>
inline constexpr bool is_typelist_v = is_typelist<T>::value;

// The concept `some_typelist<T>` is satisfied if and only if `T` is a
// specialization of `typelist`. A cv-qualified specialization, e.g.
// `typelist<> const`, does not satisfy it.
template<typename T>
concept some_typelist = is_typelist_v<T>;

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

// Checks whether `T` is an element of the typelist `List`. If `List` is not a
// typelist, `value` is `false`.
template<typename List, typename T>
struct typelist_contains_t : std::false_type {};

template<typename... Ts, typename T>
struct typelist_contains_t<typelist<Ts...>, T>
    : std::bool_constant<(... || std::same_as<T, Ts>)> {};

// Helper variable template for `typelist_contains_t`.
template<typename List, typename T>
inline constexpr bool typelist_contains_v = typelist_contains_t<List, T>::value;

// The concept `typelist_contains<List, T>` is satisfied if and only if `T` is
// an element of the typelist `List`. If `List` is not a typelist, it is not
// satisfied.
template<typename List, typename T>
concept typelist_contains = typelist_contains_v<List, T>;

// Provides the member typedef `type`, which is the element at zero-based index
// `I` of the typelist `List`. The program is ill-formed if `List` is not a
// typelist or `I >= List::size`.
template<typename List, std::size_t I>
struct typelist_at;

template<typename... Ts, std::size_t I>
struct typelist_at<typelist<Ts...>, I> {
  using type = Ts...[I];
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

// Checks whether no two elements of the typelist `List` are the same type,
// which holds for an empty typelist. If `List` is not a typelist, `value` is
// `false`.
template<typename List>
struct typelist_unique_t : std::false_type {};

template<typename... Ts>
struct typelist_unique_t<typelist<Ts...>>
    : std::bool_constant<(...
                          && (typelist_count_v<typelist<Ts...>, Ts> == 1))> {};

// Helper variable template for `typelist_unique_t`.
template<typename List>
inline constexpr bool typelist_unique_v = typelist_unique_t<List>::value;

// The concept `typelist_unique<List>` is satisfied if and only if no two
// elements of the typelist `List` are the same type, which holds for an empty
// typelist. If `List` is not a typelist, it is not satisfied.
template<typename List>
concept typelist_unique = typelist_unique_v<List>;

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
