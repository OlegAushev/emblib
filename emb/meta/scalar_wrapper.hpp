#pragma once

#include <concepts>
#include <type_traits>
#include <utility>

namespace emb {

// The concept `scalar_wrapper<T>` is satisfied if and only if `T` wraps one
// scalar and can be rebuilt from it: `scalar_of` called unqualified with a
// `T const` lvalue returns a value of a scalar type (`std::is_scalar_v`), not
// a reference to one, and `T` is constructible from that type.
//
// A wrapper, e.g. `emb::units::quantity` or `emb::clamped`, opts in by
// declaring a `scalar_of` overload that argument-dependent lookup finds: in its
// own namespace or as a friend defined in the class.
template<typename T>
concept scalar_wrapper = requires(T const& v) {
  requires std::is_scalar_v<decltype(scalar_of(v))>;
  requires std::constructible_from<T, decltype(scalar_of(v))>;
};

namespace detail {

template<typename T>
struct unwrapped {
  using type = T;
};

template<scalar_wrapper T>
struct unwrapped<T> {
  using type = decltype(scalar_of(std::declval<T const&>()));
};

} // namespace detail

// The type of the scalar that `T` wraps if `T` satisfies `scalar_wrapper`,
// otherwise `T` itself.
template<typename T>
using unwrapped_t = typename detail::unwrapped<T>::type;

} // namespace emb
