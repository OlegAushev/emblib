#pragma once

#include <concepts>
#include <type_traits>

namespace emb {

// The concept `scalar_wrapper<T>` is satisfied if and only if `T` wraps one
// scalar of type `T::value_type` and can be rebuilt from it: `T::value_type`
// is a scalar type (`std::is_scalar_v`), `T` is constructible from it, and
// `value_of` called unqualified with a `T const` lvalue returns
// `T::value_type`, not a reference to one.
//
// A wrapper, e.g. `emb::units::quantity` or `emb::clamped`, opts in by
// declaring a `value_of` overload that argument-dependent lookup finds: in its
// own namespace or as a friend defined in the class.
template<typename T>
concept scalar_wrapper = requires { typename T::value_type; }
                      && std::is_scalar_v<typename T::value_type>
                      && std::constructible_from<T, typename T::value_type>
                      && requires(T const& v) {
                           {
                             value_of(v)
                           } -> std::same_as<typename T::value_type>;
                         };

namespace detail {

template<typename T>
struct unwrapped {
  using type = T;
};

template<scalar_wrapper T>
struct unwrapped<T> {
  using type = typename T::value_type;
};

} // namespace detail

// The type of the scalar that `T` wraps if `T` satisfies `scalar_wrapper`,
// otherwise `T` itself.
template<typename T>
using unwrapped_t = typename detail::unwrapped<T>::type;

} // namespace emb
