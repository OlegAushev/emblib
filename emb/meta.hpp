#pragma once

#include <emb/meta/alternative_of.hpp>
#include <emb/meta/nth_type.hpp>
#include <emb/meta/type_index.hpp>
#include <emb/meta/typelist.hpp>
#include <emb/meta/unroll.hpp>

#include <concepts>

namespace emb {

template<typename T, typename... Ts>
concept same_as_any = (... || std::same_as<T, Ts>);

namespace detail {

template<typename... Ts>
inline constexpr bool all_same_v = true;

template<typename T, typename... Ts>
inline constexpr bool all_same_v<T, Ts...> = (... && std::same_as<T, Ts>);

} // namespace detail

// Every type in the pack is the same; vacuously true for an empty pack.
// Fully variadic so that a pack can be expanded into it directly.
template<typename... Ts>
concept all_same = detail::all_same_v<Ts...>;

template<typename... Ts>
struct overload : Ts... {
  using Ts::operator()...;

  consteval void operator()(auto) const {
    static_assert(false, "Unsupported type");
  }
};

} // namespace emb
