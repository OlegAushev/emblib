#pragma once

#include <algorithm>
#include <string_view>

#include <cstddef>

namespace emb {

// A structural string: a string literal usable as a non-type template
// argument — template<fixed_string Name>, instantiated as f<"motor.p">().
// N counts the terminating NUL, as in the literal it is built from.
//
// The character array is named `chars`, not `data`, so that `data()` and
// `size()` can be member functions: with those two, a fixed_string is also a
// valid static_assert message.
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
consteval auto concat_chars(char const (&lhs)[N], char const (&rhs)[M])
    -> fixed_string<N + M - 1>
{
  fixed_string<N + M - 1> result;
  auto it = std::copy_n(lhs, N - 1, result.chars);
  std::copy_n(rhs, M, it);
  return result;
}

} // namespace detail

// Concatenation is consteval: it exists to build diagnostic messages and
// compound names at compile time, and nothing else should be tempted to
// build strings with it at run time.
template<std::size_t N, std::size_t M>
consteval auto operator+(fixed_string<N> const& lhs, fixed_string<M> const& rhs)
{
  return detail::concat_chars(lhs.chars, rhs.chars);
}

template<std::size_t N, std::size_t M>
consteval auto operator+(char const (&lhs)[N], fixed_string<M> const& rhs)
{
  return detail::concat_chars(lhs, rhs.chars);
}

template<std::size_t N, std::size_t M>
consteval auto operator+(fixed_string<N> const& lhs, char const (&rhs)[M])
{
  return detail::concat_chars(lhs.chars, rhs);
}

} // namespace emb
