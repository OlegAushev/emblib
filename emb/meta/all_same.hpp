#pragma once

#include <concepts>

namespace emb {

namespace detail {

template<typename... Ts>
inline constexpr bool all_same_v = true;

template<typename T, typename... Ts>
inline constexpr bool all_same_v<T, Ts...> = (... && std::same_as<T, Ts>);

} // namespace detail

// The concept `all_same<Ts...>` is satisfied if and only if all types in `Ts`
// are the same type, taking into account cv-qualification and references. It
// is satisfied if `Ts` is empty.
template<typename... Ts>
concept all_same = detail::all_same_v<Ts...>;

} // namespace emb
