#pragma once

#include <concepts>
#include <type_traits>
#include <utility>

namespace emb {

// Returns a copy of an arithmetic value. In generic code outside emb, use
// `using emb::scalar_of;` and call `scalar_of(v)` to also find wrapper
// overloads.
template<typename T>
  requires std::is_arithmetic_v<T>
constexpr auto scalar_of(T const& v) noexcept
{
  return v;
}

// A non-arithmetic type `T` satisfies `scalar_wrapper` if an unqualified call
// to `scalar_of` with a `T const` lvalue returns an arithmetic type by value
// and `T` is constructible from that type.
//
// A wrapper, e.g. `emb::units::quantity` or `emb::clamped`, opts in by
// declaring a `scalar_of` overload that argument-dependent lookup finds: in its
// own namespace or as a friend defined in the class.
template<typename T>
concept scalar_wrapper =
    !std::is_arithmetic_v<std::remove_cvref_t<T>> && requires(T const& v) {
      requires std::is_arithmetic_v<decltype(scalar_of(v))>;
      requires std::constructible_from<T, decltype(scalar_of(v))>;
    };

namespace detail {

template<typename T>
struct unwrap_scalar {
  using type = T;
};

template<scalar_wrapper T>
struct unwrap_scalar<T> {
  using type = decltype(scalar_of(std::declval<T const&>()));
};

} // namespace detail

// The arithmetic type that `T` wraps if `T` satisfies `scalar_wrapper`,
// otherwise `T` itself.
template<typename T>
using unwrap_scalar_t = typename detail::unwrap_scalar<T>::type;

} // namespace emb
