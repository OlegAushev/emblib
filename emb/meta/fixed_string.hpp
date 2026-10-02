#pragma once

#include <algorithm>
#include <string_view>

#include <cstddef>

namespace emb {

// The class template `fixed_string` holds a string of `N - 1` characters and
// a terminating NUL in the array `chars`, as in the string literal the string
// is built from. It is a structural type, so a string literal can be the
// argument of a template parameter of type `fixed_string`, as in
// `template<fixed_string Name>`. `size()` is `N - 1` and `view()` covers the
// first `N - 1` characters even if a NUL is among them, e.g. in a
// default-constructed `fixed_string`, which holds `N` NULs. If the array
// passed to the constructor does not end with a NUL, `data()` is not
// NUL-terminated. The program is ill-formed if `N` is zero.
//
// The array is named `chars`, not `data`, so that `data()` and `size()` can
// be member functions: with those two, a `fixed_string` is also a valid
// `static_assert` message.
template<std::size_t N>
struct fixed_string {
  char chars[N]{};

  constexpr fixed_string() = default;

  constexpr fixed_string(char const (&str)[N])
  {
    std::copy_n(str, N, chars);
  }

  // NUL-terminated: safe to hand to anything expecting a C string.
  constexpr char const* data() const
  {
    return chars;
  }

  constexpr std::size_t size() const
  {
    return N - 1;
  }

  constexpr bool empty() const
  {
    return size() == 0;
  }

  // No implicit conversion to string_view: the view is only as good as the
  // object it points into, and an implicit one turns every temporary into a
  // dangling view.
  constexpr std::string_view view() const
  {
    return {chars, N - 1};
  }
};

template<std::size_t N>
fixed_string(char const (&)[N]) -> fixed_string<N>;

namespace detail {

template<std::size_t N, std::size_t M>
consteval fixed_string<N + M - 1> concat_chars(char const (&lhs)[N],
                                               char const (&rhs)[M])
{
  fixed_string<N + M - 1> result;
  auto it = std::copy_n(lhs, N - 1, result.chars);
  std::copy_n(rhs, M, it);
  return result;
}

} // namespace detail

// Returns the concatenation of `lhs` and `rhs`, a `fixed_string<N + M - 1>`.
// Concatenation is `consteval`, for building diagnostic messages and compound
// names at compile time; it cannot build strings at run time.
template<std::size_t N, std::size_t M>
consteval auto operator+(fixed_string<N> const& lhs, fixed_string<M> const& rhs)
{
  return detail::concat_chars(lhs.chars, rhs.chars);
}

// Equivalent to `fixed_string(lhs) + rhs`.
template<std::size_t N, std::size_t M>
consteval auto operator+(char const (&lhs)[N], fixed_string<M> const& rhs)
{
  return detail::concat_chars(lhs, rhs.chars);
}

// Equivalent to `lhs + fixed_string(rhs)`.
template<std::size_t N, std::size_t M>
consteval auto operator+(fixed_string<N> const& lhs, char const (&rhs)[M])
{
  return detail::concat_chars(lhs.chars, rhs);
}

} // namespace emb
