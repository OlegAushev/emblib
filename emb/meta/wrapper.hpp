#pragma once

#include <concepts>

namespace emb {

// The concept `some_wrapper<T>` is satisfied if and only if `T` wraps one
// value of type `T::value_type` and can be rebuilt from it: `T` is
// constructible from `T::value_type`, and `value_of` called unqualified with a
// `T const` lvalue returns `T::value_type`, not a reference to one.
//
// A wrapper, e.g. `emb::units::named_unit` or `emb::clamped`, opts in by
// declaring a `value_of` overload that argument-dependent lookup finds: in its
// own namespace or as a friend defined in the class.
template<typename T>
concept some_wrapper = requires { typename T::value_type; }
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

template<some_wrapper T>
struct unwrapped<T> {
  using type = typename T::value_type;
};

} // namespace detail

// The type of the value that `T` wraps if `T` satisfies `some_wrapper`,
// otherwise `T` itself.
template<typename T>
using unwrapped_t = typename detail::unwrapped<T>::type;

} // namespace emb
