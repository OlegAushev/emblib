#pragma once

#include <cmath>
#include <concepts>
#include <limits>

namespace emb {

// Computes the floating-point remainder of the division `x / y` exactly, with
// the same result as `std::fmod(x, y)`, including the sign of a zero result.
// Returns NaN if `x` is NaN or infinite or if `y` is zero or NaN.
// Logarithmic in |`x`| / |`y`|.
template<std::floating_point T>
consteval T fmod_trivial(T x, T y)
{
  T const ax = x < 0 ? -x : x;
  T const ay = y < 0 ? -y : y;
  if (!(ax <= std::numeric_limits<T>::max()) || !(ay > 0)) {
    return std::numeric_limits<T>::quiet_NaN();
  }
  if (ax < ay) {
    return x;
  }
  T d = ay;
  while (d <= ax - d) {
    d *= 2;
  }
  T r = ax;
  for (; d >= ay; d /= 2) {
    if (r >= d) {
      r -= d;
    }
  }
  return x < 0 ? -r : r;
}

// Computes the floating-point remainder of the division `x / y`. Returns
// `std::fmod(x, y)` at run time. During constant evaluation, returns
// `fmod_trivial(x, y)`, which gives the same result.
template<std::floating_point T>
constexpr T fmod(T x, T y)
{
  if !consteval {
    return std::fmod(x, y);
  }
  else {
    return fmod_trivial(x, y);
  }
}

// Returns the sign of `v` as `T`: -1, 0 or +1.
//
// Requires only `V{0}` and `<`, so any ordered type works; zero, negative
// zero and NaN all give 0. `T` comes first so the result type can be named
// while `V` is deduced, e.g. `sgn<float>(v)`.
template<typename T = int, typename V>
constexpr T sgn(V v)
{
  return static_cast<T>((V{0} < v) - (v < V{0}));
}

// Checks whether `n` is even.
constexpr bool iseven(std::integral auto n)
{
  return n % 2 == 0;
}

// Checks whether `n` is odd.
constexpr bool isodd(std::integral auto n)
{
  return !iseven(n);
}

// Checks whether `a` and `b` differ by less than `eps`, a tolerance stated in
// the units of `a` and `b`. Returns `false` if any argument is NaN.
template<typename T>
constexpr bool approx(T a, T b, T eps)
{
  return (a < b ? b - a : a - b) < eps;
}

} // namespace emb
