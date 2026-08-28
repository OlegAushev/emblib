#pragma once

#include <cstddef>
#include <utility>

namespace emb {

namespace detail {

// T for any index: expanding replicated<T, I>... over an index pack repeats
// T once per index.
template<typename T, std::size_t>
using replicated = T;

} // namespace detail

// Template<T, T, ...>: a variadic class template applied to N copies of T,
// for a count known as a constant rather than a spelled-out list.
template<template<typename...> class Template, typename T, typename Indices>
struct replicate;

template<template<typename...> class Template, typename T, std::size_t... I>
struct replicate<Template, T, std::index_sequence<I...>> {
  using type = Template<detail::replicated<T, I>...>;
};

template<template<typename...> class Template, typename T, std::size_t N>
using replicate_t =
    typename replicate<Template, T, std::make_index_sequence<N>>::type;

} // namespace emb
