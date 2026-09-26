#pragma once

#include <cstddef>
#include <utility>

namespace emb {

namespace detail {

// Alias template that denotes `T` for any index, so that expanding it over an
// index pack repeats `T` once per index.
template<typename T, std::size_t>
using replicated = T;

} // namespace detail

// Provides the member typedef `type`, which is `Template<T, T, ...>` with one
// `T` per element of the `std::index_sequence` `Indices`. If `Indices` is not
// a `std::index_sequence` or `Template` does not accept these arguments, the
// program is ill-formed.
template<template<typename...> class Template, typename T, typename Indices>
struct replicate;

template<template<typename...> class Template, typename T, std::size_t... I>
struct replicate<Template, T, std::index_sequence<I...>> {
  using type = Template<detail::replicated<T, I>...>;
};

// `replicate_t<Template, T, N>` is `Template<T, T, ...>` with `N` copies of
// `T`, for a count known as a constant rather than a spelled-out list. If `N`
// is zero, it is `Template<>`. The program is ill-formed if `Template` does
// not accept these arguments.
template<template<typename...> class Template, typename T, std::size_t N>
using replicate_t =
    typename replicate<Template, T, std::make_index_sequence<N>>::type;

} // namespace emb
